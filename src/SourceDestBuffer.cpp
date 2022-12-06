/*
 * SourceDestBuffer.cpp - implementation of public functions of the SourceDestBuffer class.
 *
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

/// @file SourceDestBuffer.cpp

#include "SourceDestBufferImpl.h"

using namespace e57;

// Put this function first so we can reference the code in doxygen using @skip
/// @brief Check whether SourceDestBuffer class invariant is true
void SourceDestBuffer::checkInvariant( bool /*doRecurse*/ ) const
{
   // Stride must be >= a memory type dependent value
   size_t min_stride = 0;

   switch ( memoryRepresentation() )
   {
      case Int8:
         min_stride = 1;
         break;
      case UInt8:
         min_stride = 1;
         break;
      case Int16:
         min_stride = 2;
         break;
      case UInt16:
         min_stride = 2;
         break;
      case Int32:
         min_stride = 4;
         break;
      case UInt32:
         min_stride = 4;
         break;
      case Int64:
         min_stride = 8;
         break;
      case Bool:
         min_stride = 1;
         break;
      case Real32:
         min_stride = 4;
         break;
      case Real64:
         min_stride = 8;
         break;
      case UString:
         min_stride = sizeof( ustring );
         break;
      default:
         throw E57_EXCEPTION1( ErrorInvarianceViolation );
   }

   if ( stride() < min_stride )
   {
      throw E57_EXCEPTION1( ErrorInvarianceViolation );
   }
}

/*!
@class e57::SourceDestBuffer

@brief A memory buffer to transfer data to/from a CompressedVectorNode in a block.

@details
The SourceDestBuffer is an encapsulation of a buffer in memory that will transfer data to/from a
field in a CompressedVectorNode. The API user is responsible for creating the actual memory buffer,
describing it correctly to the API, making sure it exists during the transfer period, and destroying
it after the transfer is complete. Additionally, the SourceDestBuffer has information that specifies
the connection to the CompressedVectorNode field (i.e. the field's path name in the prototype).

The type of buffer element may be an assortment of built-in C++ memory types. There are all
combinations of signed/unsigned and 8/16/32/64 bit integers (except unsigned 64bit integer, which is
not supported in the ASTM standard), bool, float, double, as well as a vector of variable length
unicode strings. The compiler selects the appropriate constructor automatically based on the type of
the buffer array. However, the API user is responsible for reporting the correct length and stride
options (otherwise unspecified behavior can occur).

The connection of the SourceDestBuffer to a CompressedVectorNode field is established by specifying
the pathName. There are several options to this connection: doConversion and doScaling, which are
described in the constructor documentation.

@section sourcedestbuffer_invariant Class Invariant
A class invariant is a list of statements about an object that are always true before and after any
operation on the object. An invariant is useful for testing correct operation of an implementation.
Statements in an invariant can involve only externally visible state, or can refer to internal
implementation-specific state that is not visible to the API user. The following C++ code checks
externally visible state for consistency and throws an exception if the invariant is violated:

@dontinclude SourceDestBuffer.cpp
@skip void SourceDestBuffer::checkInvariant
@until ^}

@see Node
*/

/*!
@brief Designate buffers to transfer data to/from a CompressedVectorNode in a block.

@param [in] destImageFile The ImageFile where the new node will eventually be stored.
@param [in] pathName The pathname of the field in CompressedVectorNode that will transfer data
to/from.
@param [in] b The caller allocated memory buffer.
@param [in] capacity The total number of memory elements in buffer @a b.
@param [in] doConversion Will a conversion be attempted between memory and ImageFile
representations.
@param [in] doScaling In a ScaledInteger field, do memory elements hold scaled values, if false they
hold raw values.
@param [in] stride The number of bytes between memory elements.  If zero, defaults to sizeof memory
element.

@details
This overloaded form of the SourceDestBuffer constructor declares a buffer @a b to be the
source/destination of a transfer of values stored in a CompressedVectorNode.

The @a pathName will be used to identify a Node in the prototype that will get/receive data from
this buffer. The @a pathName may be an absolute path name (e.g. "/cartesianX") or a path name
relative to the root of the prototype (i.e. the absolute path name without the leading "/", for
example: "cartesianX").

The type of @a b is used to determine the MemoryRepresentation of the SourceDestBuffer. The buffer
@a b may be used for multiple block transfers. See discussions of operation of SourceDestBuffer
attributes in SourceDestBuffer::memoryRepresentation, SourceDestBuffer::capacity,
SourceDestBuffer::doConversion, and SourceDestBuffer::doScaling, and SourceDestBuffer::stride.

The API user is responsible for ensuring that the lifetime of the @a b memory buffer exceeds the
time that it is used in transfers (i.e. the E57 Foundation Implementation cannot detect that the
buffer been destroyed).

The @a capacity must match the capacity of all other SourceDestBuffers that will participate in a
transfer with a CompressedVectorNode.

@pre The @a destImageFile must be open (i.e. destImageFile.isOpen() must be true).
@pre The stride must be >= sizeof(*b)

@throw ::ErrorBadAPIArgument
@throw ::ErrorBadPathName
@throw ::ErrorBadBuffer
@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see ImageFile::reader, ImageFile::writer,
CompressedVectorReader::read(std::vector<SourceDestBuffer>&),
CompressedVectorWriter::write(std::vector<SourceDestBuffer>&)
*/
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, int8_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<int8_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, uint8_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<uint8_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, int16_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<int16_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, uint16_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<uint16_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, int32_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<int32_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, uint32_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<uint32_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, int64_t *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<int64_t>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, bool *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<bool>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, float *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<float>( b, stride );
}

/// @overload
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName, double *b,
                                    const size_t capacity, bool doConversion, bool doScaling,
                                    size_t stride ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, capacity, doConversion,
                                    doScaling ) )
{
   impl_->setTypeInfo<double>( b, stride );
}

/*!
@brief Designate vector of strings to transfer data to/from a CompressedVector as a block.

@param [in] destImageFile The ImageFile where the new node will eventually be stored.
@param [in] pathName The pathname of the field in CompressedVectorNode that will transfer data
to/from.
@param [in] b The caller created vector of ustrings to transfer from/to.

@details
This overloaded form of the SourceDestBuffer constructor declares a vector<ustring> to be the
source/destination of a transfer of StringNode values stored in a CompressedVectorNode.

The @a pathName will be used to identify a Node in the prototype that will get/receive data from
this buffer. The @a pathName may be an absolute path name (e.g. "/cartesianX") or a path name
relative to the root of the prototype (i.e. the absolute path name without the leading "/", for
example: "cartesianX").

The @a b->size() must match capacity of all other SourceDestBuffers that will participate in a
transfer with a CompressedVectorNode (string or any other type of buffer). In a read into the
SourceDestBuffer, the previous contents of the strings in the vector are lost, and the memory space
is potentially freed. The @a b->size() of the vector will not be changed. It is an error to request
a read/write of more records that @a b->size() (just as it would be for buffers of integer types).
The API user is responsible for ensuring that the lifetime of the @a b vector exceeds the time that
it is used in transfers (i.e. the E57 Foundation Implementation cannot detect that the buffer been
destroyed).

@pre b.size() must be > 0.
@pre The @a destImageFile must be open (i.e. destImageFile.isOpen() must be true).

@throw ::ErrorBadAPIArgument
@throw ::ErrorBadPathName
@throw ::ErrorBadBuffer
@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see SourceDestBuffer::doConversion for discussion on representations compatible with string
SourceDestBuffers.
*/
SourceDestBuffer::SourceDestBuffer( ImageFile destImageFile, const ustring &pathName,
                                    std::vector<ustring> *b ) :
   impl_( new SourceDestBufferImpl( destImageFile.impl(), pathName, b ) )
{
}

/*!
@brief Get path name in prototype that this SourceDestBuffer will transfer data to/from.

@details
The prototype of a CompressedVectorNode describes the fields that are in each record. This function
returns the path name of the node in the prototype tree that this SourceDestBuffer will write/read.
The correctness of this path name is checked when this SourceDestBuffer is associated with a
CompressedVectorNode (either in CompressedVectorNode::writer,
CompressedVectorWriter::write(std::vector<SourceDestBuffer>&, unsigned),
CompressedVectorNode::reader, CompressedVectorReader::read(std::vector<SourceDestBuffer>&)).

@post No visible state is modified.

@return Path name in prototype that this SourceDestBuffer will transfer data to/from.

@throw ::ErrorInternal All objects in undocumented state

@see CompressedVector, CompressedVectorNode::prototype
*/
ustring SourceDestBuffer::pathName() const
{
   return impl_->pathName();
}

/*!
@brief Get memory representation of the elements in this SourceDestBuffer.

@details
The memory representation is deduced from which overloaded SourceDestBuffer constructor was used.
The memory representation is independent of the type and minimum/maximum bounds of the node in the
prototype that the SourceDestBuffer will transfer to/from. However, some combinations will result in
an error if doConversion is not requested (e.g. ::Int16 and FloatNode).

Some combinations risk an error occurring during a write, if a value is too large (e.g. writing an
::Int16 memory representation to an IntegerNode with minimum=-1024 maximum=1023). Some combinations
risk an error occurring during a read, if a value is too large (e.g. reading an IntegerNode with
minimum=-1024 maximum=1023 int an ::Int8 memory representation). Some combinations are never
possible (e.g. ::Int16 and StringNode).

@post No visible state is modified.

@return Memory representation of the elements in buffer.

@throw ::ErrorInternal All objects in undocumented state

@see MemoryRepresentation, NodeType
*/
MemoryRepresentation SourceDestBuffer::memoryRepresentation() const
{
   return impl_->memoryRepresentation();
}

/*!
@brief Get total capacity of buffer.

@details
The API programmer is responsible for correctly specifying the length of a buffer. This function
returns that declared length. If the length is incorrect (in particular, too long) memory may be
corrupted or erroneous values written.

@post No visible state is modified.

@return Total capacity of buffer.

@throw ::ErrorInternal All objects in undocumented state
*/
size_t SourceDestBuffer::capacity() const
{
   return impl_->capacity();
}

/*!
@brief Get whether conversions will be performed to match the memory type of buffer.

@details
The API user must explicitly request conversion between basic representation groups in memory and on
the disk. The four basic representation groups are: integer, boolean, floating point, and string.
There is no distinction between integer and boolean groups on the disk (they both use IntegerNode).
A explicit request for conversion between single and double precision floating point representations
is not required.

The most useful conversion is between integer and floating point representation groups. Conversion
from integer to floating point representations cannot result in an overflow, and is usually
loss-less (except for extremely large integers). Conversion from floating point to integer
representations can result in an overflow, and can be lossy.

Conversion between any of the integer, boolean, and floating point representation groups is
supported. No conversion from the string to any other representation group is possible. Missing or
unsupported conversions are detected when the first transfer is attempted (i.e. not when the
CompressedVectorReader or CompressedVectorWriter is created).

@post No visible state is modified.

@return true if conversions will be performed to match the memory type of buffer.

@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state
*/
bool SourceDestBuffer::doConversion() const
{
   return impl_->doConversion();
}

/*!
@brief Get whether scaling will be performed for ScaledIntegerNode transfers.

@details
The doScaling option only applies to ScaledIntegerNodes stored in a CompressedVectorNode on the disk
(it is ignored if a ScaledIntegerNode is not involved).

As a convenience, an E57 Foundation Implementation can perform scaling of data so that the API user
can manipulate scaledValues rather than rawValues. For a reader, the scaling process is: scaledValue
= (rawValue * scale) + offset. For a writer, the scaling process is reversed: rawValue =
(scaledValue - offset) / scale. The result performing a scaling in a reader (or "unscaling" in a
writer) is always a floating point number. This floating point number may have to be converted to be
compatible with the destination representation. If the destination representation is not floating
point, there is a risk of violating declared min/max bounds. Because of this risk, it is recommended
that scaling only be requested for reading scaledValues from ScaledIntegerNodes into floating point
numbers in memory.

It is also possible (and perhaps safest of all) to never request that scaling be performed, and
always deal with rawValues outside the API. Note this does not mean that ScaledIntegerNodes should
be avoided. ScaledIntgerNodes are essential for encoding numeric data with fractional parts in
CompressedVectorNodes. Because the ASTM E57 format recommends that SI units without prefix be used
(i.e. meters, not milli-meters or micro-furlongs), almost every measured value will have a
fractional part.

@post No visible state is modified.

@return true if scaling will be performed for ScaledInteger transfers.

@throw ::ErrorInternal All objects in undocumented state

@see ScaledIntegerNode
*/
bool SourceDestBuffer::doScaling() const
{
   return impl_->doScaling();
}

/*!
@brief Get number of bytes between consecutive memory elements in buffer

@details
Elements in a memory buffer do not have to be consecutive. They can also be spaced at regular
intervals. This allows a value to be picked out of an array of C++ structures (the stride would be
the size of the structure). In the case that the element values are stored consecutively in memory,
the stride equals the size of the memory representation of the element.

@post No visible state is modified.

@return Number of bytes between consecutive memory elements in buffer
*/
size_t SourceDestBuffer::stride() const
{
   return impl_->stride();
}

/*!
@brief Diagnostic function to print internal state of object to output stream in an indented format.
@copydetails Node::dump()
*/
#ifdef E57_DEBUG
void SourceDestBuffer::dump( int indent, std::ostream &os ) const
{
   impl_->dump( indent, os );
}
#else
void SourceDestBuffer::dump( int indent, std::ostream &os ) const
{
}
#endif