/*
 * Original work Copyright 2009 - 2010 Kevin Ackley (kackley@gwi.net)
 * Modified work Copyright 2018 - 2020 Andy Maloney <asmaloney@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "CompressedVectorReaderImpl.h"
#include "CheckedFile.h"
#include "CompressedVectorNodeImpl.h"
#include "ImageFileImpl.h"
#include "Packet.h"
#include "SectionHeaders.h"
#include "SourceDestBufferImpl.h"

namespace e57
{
   CompressedVectorReaderImpl::CompressedVectorReaderImpl( std::shared_ptr<CompressedVectorNodeImpl> cvi,
                                                           std::vector<SourceDestBuffer> &dbufs ) :
      isOpen_( false ), // set to true when succeed below
      cVector_( cvi )
   {
#ifdef E57_MAX_VERBOSE
      std::cout << "CompressedVectorReaderImpl() called" << std::endl; //???
#endif
      checkImageFileOpen( __FILE__, __LINE__, static_cast<const char *>( __FUNCTION__ ) );

      /// Allow reading of a completed CompressedVector, whether file is being read
      /// or currently being written.
      ///??? what other situations need checking for?
      ///??? check if CV not yet written to?
      ///??? file in error state?

      /// Empty dbufs is an error
      if ( dbufs.empty() )
      {
         throw E57_EXCEPTION2( E57_ERROR_BAD_API_ARGUMENT,
                               "imageFileName=" + cVector_->imageFileName() + " cvPathName=" + cVector_->pathName() );
      }

      /// Get CompressedArray's prototype node (all array elements must match this
      /// type)
      proto_ = cVector_->getPrototype();

      /// Check dbufs well formed (matches proto exactly)
      setBuffers( dbufs );

      /// For each dbuf, create an appropriate Decoder based on the cVector_
      /// attributes
      for ( unsigned i = 0; i < dbufs_.size(); i++ )
      {
         std::vector<SourceDestBuffer> theDbuf;
         theDbuf.push_back( dbufs.at( i ) );

         std::shared_ptr<Decoder> decoder = Decoder::DecoderFactory( i, cVector_.get(), theDbuf, ustring() );

         /// Calc which stream the given path belongs to.  This depends on position
         /// of the node in the proto tree.
         NodeImplSharedPtr readNode = proto_->get( dbufs.at( i ).pathName() );
         uint64_t bytestreamNumber = 0;
         if ( !proto_->findTerminalPosition( readNode, bytestreamNumber ) )
         {
            throw E57_EXCEPTION2( E57_ERROR_INTERNAL, "dbufIndex=" + toString( i ) );
         }

         channels_.emplace_back( dbufs.at( i ), decoder, static_cast<unsigned>( bytestreamNumber ),
                                 cVector_->childCount() );
      }

      recordCount_ = 0;

      /// Get how many records are actually defined
      maxRecordCount_ = cvi->childCount();

      ImageFileImplSharedPtr imf( cVector_->destImageFile_ );

      //??? what if fault in this constructor?
      cache_ = new PacketReadCache( imf->file_, 32 );

      /// Read CompressedVector section header
      CompressedVectorSectionHeader sectionHeader;
      uint64_t sectionLogicalStart = cVector_->getBinarySectionLogicalStart();
      if ( sectionLogicalStart == 0 )
      {
         //??? should have caught this before got here, in XML read, get this if CV
         // wasn't written to
         // by writer.
         throw E57_EXCEPTION2( E57_ERROR_INTERNAL,
                               "imageFileName=" + cVector_->imageFileName() + " cvPathName=" + cVector_->pathName() );
      }
      imf->file_->seek( sectionLogicalStart, CheckedFile::Logical );
      imf->file_->read( reinterpret_cast<char *>( &sectionHeader ), sizeof( sectionHeader ) );

#ifdef E57_DEBUG
      sectionHeader.verify( imf->file_->length( CheckedFile::Physical ) );
#endif

      /// Pre-calc end of section, so can tell when we are out of packets.
      sectionEndLogicalOffset_ = sectionLogicalStart + sectionHeader.sectionLogicalLength;

      /// Convert physical offset to first data packet to logical
      uint64_t dataLogicalOffset = imf->file_->physicalToLogical( sectionHeader.dataPhysicalOffset );

      /// Verify that packet given by dataPhysicalOffset is actually a data packet,
      /// init channels
      {
         char *anyPacket = nullptr;
         std::unique_ptr<PacketLock> packetLock = cache_->lock( dataLogicalOffset, anyPacket );

         auto dpkt = reinterpret_cast<DataPacket *>( anyPacket );

         /// Double check that have a data packet
         if ( dpkt->header.packetType != DATA_PACKET )
         {
            throw E57_EXCEPTION2( E57_ERROR_BAD_CV_PACKET, "packetType=" + toString( dpkt->header.packetType ) );
         }

         /// Have good packet, initialize channels
         for ( auto &channel : channels_ )
         {
            channel.currentPacketLogicalOffset = dataLogicalOffset;
            channel.currentBytestreamBufferIndex = 0;
            channel.currentBytestreamBufferLength = dpkt->getBytestreamBufferLength( channel.bytestreamNumber );
         }
      }

      /// Just before return (and can't throw) increment reader count  ??? safer
      /// way to assure don't miss close?
      imf->incrReaderCount();

      /// If get here, the reader is open
      isOpen_ = true;
   }

   CompressedVectorReaderImpl::~CompressedVectorReaderImpl()
   {
#ifdef E57_MAX_VERBOSE
      std::cout << "~CompressedVectorReaderImpl() called" << std::endl; //???
                                                                        // dump(4);
#endif

      if ( isOpen_ )
      {
         try
         {
            close(); ///??? what if already closed?
         }
         catch ( ... )
         {
            //??? report?
         }
      }
   }

   void CompressedVectorReaderImpl::setBuffers( std::vector<SourceDestBuffer> &dbufs )
   {
      /// don't checkImageFileOpen
      /// don't checkReaderOpen

      /// Check dbufs well formed: no dups, no extra, missing is ok
      proto_->checkBuffers( dbufs, true );

      /// If had previous dbufs_, check to see if new ones have changed in
      /// incompatible way
      if ( !dbufs_.empty() )
      {
         if ( dbufs_.size() != dbufs.size() )
         {
            throw E57_EXCEPTION2( E57_ERROR_BUFFERS_NOT_COMPATIBLE,
                                  "oldSize=" + toString( dbufs_.size() ) + " newSize=" + toString( dbufs.size() ) );
         }
         for ( size_t i = 0; i < dbufs_.size(); i++ )
         {
            std::shared_ptr<SourceDestBufferImpl> oldBuf = dbufs_[i].impl();
            std::shared_ptr<SourceDestBufferImpl> newBuf = dbufs[i].impl();

            /// Throw exception if old and new not compatible
            oldBuf->checkCompatible( newBuf );
         }
      }

      dbufs_ = dbufs;
   }

   unsigned CompressedVectorReaderImpl::read( std::vector<SourceDestBuffer> &dbufs )
   {
      /// don't checkImageFileOpen(__FILE__, __LINE__, __FUNCTION__), read() will
      /// do it

      checkReaderOpen( __FILE__, __LINE__, static_cast<const char *>( __FUNCTION__ ) );

      /// Check compatible with current dbufs
      setBuffers( dbufs );

      return ( read() );
   }

   unsigned CompressedVectorReaderImpl::read()
   {
#ifdef E57_MAX_VERBOSE
      std::cout << "CompressedVectorReaderImpl::read() called" << std::endl; //???
#endif
      checkImageFileOpen( __FILE__, __LINE__, static_cast<const char *>( __FUNCTION__ ) );
      checkReaderOpen( __FILE__, __LINE__, static_cast<const char *>( __FUNCTION__ ) );

      /// Rewind all dbufs so start writing to them at beginning
      for ( auto &dbuf : dbufs_ )
      {
         dbuf.impl()->rewind();
      }

      /// Allow decoders to use data they already have in their queue to fill newly
      /// empty dbufs This helps to keep decoder input queues smaller, which
      /// reduces backtracking in the packet cache.
      for ( auto &channel : channels_ )
      {
         channel.decoder->inputProcess( nullptr, 0 );
      }

      /// Loop until every dbuf is full or we have reached end of the binary
      /// section.
      while ( true )
      {
         /// Find the earliest packet position for channels that are still hungry
         /// It's important to call inputProcess of the decoders before this call,
         /// so current hungriness level is reflected.
         uint64_t earliestPacketLogicalOffset = earliestPacketNeededForInput();

         /// If nobody's hungry, we are done with the read
         if ( earliestPacketLogicalOffset == E57_UINT64_MAX )
         {
            break;
         }

         /// Feed packet to the hungry decoders
         feedPacketToDecoders( earliestPacketLogicalOffset );
      }

      /// Verify that each channel produced the same number of records
      unsigned outputCount = 0;
      for ( unsigned i = 0; i < channels_.size(); i++ )
      {
         DecodeChannel *chan = &channels_[i];
         if ( i == 0 )
         {
            outputCount = chan->dbuf.impl()->nextIndex();
         }
         else
         {
            if ( outputCount != chan->dbuf.impl()->nextIndex() )
            {
               throw E57_EXCEPTION2( E57_ERROR_INTERNAL, "outputCount=" + toString( outputCount ) + " nextIndex=" +
                                                            toString( chan->dbuf.impl()->nextIndex() ) );
            }
         }
      }

      /// Return number of records transferred to each dbuf.
      return outputCount;
   }

   uint64_t CompressedVectorReaderImpl::earliestPacketNeededForInput() const
   {
      uint64_t earliestPacketLogicalOffset = E57_UINT64_MAX;
#ifdef E57_MAX_VERBOSE
      unsigned earliestChannel = 0;
#endif

      for ( unsigned i = 0; i < channels_.size(); i++ )
      {
         const DecodeChannel *chan = &channels_[i];

         /// Test if channel needs more input.
         /// Important to call inputProcess just before this, so these tests work.
         if ( !chan->isOutputBlocked() && !chan->inputFinished )
         {
            /// Check if earliest so far
            if ( chan->currentPacketLogicalOffset < earliestPacketLogicalOffset )
            {
               earliestPacketLogicalOffset = chan->currentPacketLogicalOffset;
#ifdef E57_MAX_VERBOSE
               earliestChannel = i;
#endif
            }
         }
      }
#ifdef E57_MAX_VERBOSE
      if ( earliestPacketLogicalOffset == E57_UINT64_MAX )
      {
         std::cout << "earliestPacketNeededForInput returning none found" << std::endl;
      }
      else
      {
         std::cout << "earliestPacketNeededForInput returning " << earliestPacketLogicalOffset << " for channel["
                   << earliestChannel << "]" << std::endl;
      }
#endif
      return earliestPacketLogicalOffset;
   }

   DataPacket *CompressedVectorReaderImpl::dataPacket( uint64_t inLogicalOffset ) const
   {
      char *packet = nullptr;

      std::unique_ptr<PacketLock> packetLock = cache_->lock( inLogicalOffset, packet );

      return reinterpret_cast<DataPacket *>( packet );
   }

   inline bool _alreadyReadPacket( const DecodeChannel &channel, uint64_t currentPacketLogicalOffset )
   {
      return ( ( channel.currentPacketLogicalOffset != currentPacketLogicalOffset ) || channel.isOutputBlocked() );
   }

   void CompressedVectorReaderImpl::feedPacketToDecoders( uint64_t currentPacketLogicalOffset )
   {
      // Get packet at currentPacketLogicalOffset into memory.
      auto dpkt = dataPacket( currentPacketLogicalOffset );

      // Double check that have a data packet.  Should have already determined
      // this.
      if ( dpkt->header.packetType != DATA_PACKET )
      {
         throw E57_EXCEPTION2( E57_ERROR_INTERNAL, "packetType=" + toString( dpkt->header.packetType ) );
      }

      // Read earliest packet into cache and send data to decoders with unblocked
      // output

      bool anyChannelHasExhaustedPacket = false;
      uint64_t nextPacketLogicalOffset = E57_UINT64_MAX;

      // Feed bytestreams to channels with unblocked output that are reading from
      // this packet
      for ( DecodeChannel &channel : channels_ )
      {
         // Skip channels that have already read this packet.
         if ( _alreadyReadPacket( channel, currentPacketLogicalOffset ) )
         {
            continue;
         }

         // Get bytestream buffer for this channel from packet
         unsigned int bsbLength = 0;
         const char *bsbStart = dpkt->getBytestream( channel.bytestreamNumber, bsbLength );

         // Double check we are not off end of buffer
         if ( channel.currentBytestreamBufferIndex > bsbLength )
         {
            throw E57_EXCEPTION2( E57_ERROR_INTERNAL,
                                  "currentBytestreamBufferIndex =" + toString( channel.currentBytestreamBufferIndex ) +
                                     " bsbLength=" + toString( bsbLength ) );
         }

         // Calc where we are in the buffer
         const char *uneatenStart = &bsbStart[channel.currentBytestreamBufferIndex];
         const size_t uneatenLength = bsbLength - channel.currentBytestreamBufferIndex;

         if ( &uneatenStart[uneatenLength] > &bsbStart[bsbLength] )
         {
            throw E57_EXCEPTION2( E57_ERROR_INTERNAL, "uneatenLength=" + toString( uneatenLength ) +
                                                         " bsbLength=" + toString( bsbLength ) );
         }

         // Feed into decoder
         const size_t bytesProcessed = channel.decoder->inputProcess( uneatenStart, uneatenLength );

#ifdef E57_MAX_VERBOSE
         std::cout << "  stream[" << channel.bytestreamNumber << "]: feeding decoder " << uneatenLength << " bytes"
                   << std::endl;

         if ( uneatenLength == 0 )
         {
            channel.dump( 8 );
         }

         std::cout << "  stream[" << channel.bytestreamNumber << "]: bytesProcessed=" << bytesProcessed << std::endl;
#endif

         // Adjust counts of bytestream location
         channel.currentBytestreamBufferIndex += bytesProcessed;

         // Check if this channel has exhausted its bytestream buffer in this
         // packet
         if ( channel.isInputBlocked() )
         {
#ifdef E57_MAX_VERBOSE
            std::cout << "  stream[" << channel.bytestreamNumber << "] has exhausted its input in current packet"
                      << std::endl;
#endif
            anyChannelHasExhaustedPacket = true;
            nextPacketLogicalOffset = currentPacketLogicalOffset + dpkt->header.packetLogicalLengthMinus1 + 1;
         }
      }

      // Skip over any index or empty packets to next data packet.
      nextPacketLogicalOffset = findNextDataPacket( nextPacketLogicalOffset );

      // If no channel is exhausted, we're done
      if ( !anyChannelHasExhaustedPacket )
      {
         return;
      }

      // Some channel has exhausted this packet, so find next data packet and
      // update currentPacketLogicalOffset for all interested channels.

      if ( nextPacketLogicalOffset < E57_UINT64_MAX )
      { //??? huh?
         // Get packet at nextPacketLogicalOffset into memory.
         dpkt = dataPacket( nextPacketLogicalOffset );

         // Got a data packet, update the channels with exhausted input
         for ( DecodeChannel &channel : channels_ )
         {
            // Skip channels that have already read this packet.
            if ( _alreadyReadPacket( channel, currentPacketLogicalOffset ) )
            {
               continue;
            }

            channel.currentPacketLogicalOffset = nextPacketLogicalOffset;
            channel.currentBytestreamBufferIndex = 0;

            // It is OK if the next packet doesn't contain any data for this
            // channel, will skip packet on next iter of loop
            channel.currentBytestreamBufferLength = dpkt->getBytestreamBufferLength( channel.bytestreamNumber );

#ifdef E57_MAX_VERBOSE
            std::cout << "  set new stream buffer for channel[" << channel.bytestreamNumber
                      << "], length=" << channel.currentBytestreamBufferLength << std::endl;
#endif
            // ??? perform flush if new packet flag set?
         }
      }
      else
      {
         // Reached end without finding data packet, mark exhausted channels as
         // finished
#ifdef E57_MAX_VERBOSE
         std::cout << "  at end of data packets" << std::endl;
#endif
         if ( nextPacketLogicalOffset >= sectionEndLogicalOffset_ )
         {
            for ( DecodeChannel &channel : channels_ )
            {
               // Skip channels that have already read this packet.
               if ( _alreadyReadPacket( channel, currentPacketLogicalOffset ) )
               {
                  continue;
               }

#ifdef E57_MAX_VERBOSE
               std::cout << "  Marking channel[" << channel.bytestreamNumber << "] as finished" << std::endl;
#endif
               channel.inputFinished = true;
            }
         }
      }
   }

   uint64_t CompressedVectorReaderImpl::findNextDataPacket( uint64_t nextPacketLogicalOffset )
   {
#ifdef E57_MAX_VERBOSE
      std::cout << "  searching for next data packet, nextPacketLogicalOffset=" << nextPacketLogicalOffset
                << " sectionEndLogicalOffset=" << sectionEndLogicalOffset_ << std::endl;
#endif

      /// Starting at nextPacketLogicalOffset, search for next data packet until
      /// hit end of binary section.
      while ( nextPacketLogicalOffset < sectionEndLogicalOffset_ )
      {
         char *anyPacket = nullptr;

         std::unique_ptr<PacketLock> packetLock = cache_->lock( nextPacketLogicalOffset, anyPacket );

         /// Guess it's a data packet, if not continue to next packet
         auto dpkt = reinterpret_cast<const DataPacket *>( anyPacket );

         if ( dpkt->header.packetType == DATA_PACKET )
         {
#ifdef E57_MAX_VERBOSE
            std::cout << "  Found next data packet at nextPacketLogicalOffset=" << nextPacketLogicalOffset << std::endl;
#endif
            return nextPacketLogicalOffset;
         }

         /// All packets have length in same place, so can use the field to skip to
         /// next packet.
         nextPacketLogicalOffset += dpkt->header.packetLogicalLengthMinus1 + 1;
      }

      /// Ran off end of section, so return failure code.
      return E57_UINT64_MAX;
   }

   void CompressedVectorReaderImpl::seek( uint64_t /*recordNumber*/ )
   {
      checkImageFileOpen( __FILE__, __LINE__, static_cast<const char *>( __FUNCTION__ ) );

      ///!!! implement
      throw E57_EXCEPTION1( E57_ERROR_NOT_IMPLEMENTED );
   }

   bool CompressedVectorReaderImpl::isOpen() const
   {
      /// don't checkImageFileOpen(__FILE__, __LINE__, __FUNCTION__), or
      /// checkReaderOpen()
      return ( isOpen_ );
   }

   std::shared_ptr<CompressedVectorNodeImpl> CompressedVectorReaderImpl::compressedVectorNode() const
   {
      return ( cVector_ );
   }

   void CompressedVectorReaderImpl::close()
   {
      /// Before anything that can throw, decrement reader count
      ImageFileImplSharedPtr imf( cVector_->destImageFile_ );
      imf->decrReaderCount();

      checkImageFileOpen( __FILE__, __LINE__, static_cast<const char *>( __FUNCTION__ ) );

      /// No error if reader not open
      if ( !isOpen_ )
      {
         return;
      }

      /// Destroy decoders
      channels_.clear();

      delete cache_;
      cache_ = nullptr;

      isOpen_ = false;
   }

   void CompressedVectorReaderImpl::checkImageFileOpen( const char *srcFileName, int srcLineNumber,
                                                        const char *srcFunctionName ) const
   {
      // unimplemented...
   }

   void CompressedVectorReaderImpl::checkReaderOpen( const char *srcFileName, int srcLineNumber,
                                                     const char *srcFunctionName ) const
   {
      if ( !isOpen_ )
      {
         throw E57Exception( E57_ERROR_READER_NOT_OPEN,
                             "imageFileName=" + cVector_->imageFileName() + " cvPathName=" + cVector_->pathName(),
                             srcFileName, srcLineNumber, srcFunctionName );
      }
   }

#ifdef E57_DEBUG
   void CompressedVectorReaderImpl::dump( int indent, std::ostream &os )
   {
      os << space( indent ) << "isOpen:" << isOpen_ << std::endl;

      for ( unsigned i = 0; i < dbufs_.size(); i++ )
      {
         os << space( indent ) << "dbufs[" << i << "]:" << std::endl;
         dbufs_[i].dump( indent + 4, os );
      }

      os << space( indent ) << "cVector:" << std::endl;
      cVector_->dump( indent + 4, os );

      os << space( indent ) << "proto:" << std::endl;
      proto_->dump( indent + 4, os );

      for ( unsigned i = 0; i < channels_.size(); i++ )
      {
         os << space( indent ) << "channels[" << i << "]:" << std::endl;
         channels_[i].dump( indent + 4, os );
      }

      os << space( indent ) << "recordCount:             " << recordCount_ << std::endl;
      os << space( indent ) << "maxRecordCount:          " << maxRecordCount_ << std::endl;
      os << space( indent ) << "sectionEndLogicalOffset: " << sectionEndLogicalOffset_ << std::endl;
   }
#endif

}
