/*
 * VectorNode.cpp - implementation of public functions of the VectorNode class.
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

/// @file VectorNode.cpp

#include "StringFunctions.h"
#include "VectorNodeImpl.h"

using namespace e57;

// Put this function first so we can reference the code in doxygen using @skip
/*!
@brief Check whether VectorNode class invariant is true
@copydetails IntegerNode::checkInvariant()
*/
void VectorNode::checkInvariant( bool doRecurse, bool doUpcast ) const
{
   // If destImageFile not open, can't test invariant (almost every call would throw)
   if ( !destImageFile().isOpen() )
   {
      return;
   }

   // If requested, call Node::checkInvariant
   if ( doUpcast )
   {
      static_cast<Node>( *this ).checkInvariant( false, false );
   }

   // Check each child
   for ( int64_t i = 0; i < childCount(); i++ )
   {
      Node child = get( i );

      // If requested, check children recursively
      if ( doRecurse )
      {
         child.checkInvariant( doRecurse, true );
      }

      // Child's parent must be this
      if ( static_cast<Node>( *this ) != child.parent() )
      {
         throw E57_EXCEPTION1( ErrorInvarianceViolation );
      }

      // Child's elementName must be defined
      if ( !isDefined( child.elementName() ) )
      {
         throw E57_EXCEPTION1( ErrorInvarianceViolation );
      }

      // Getting child by element name must yield same child
      Node n = get( child.elementName() );
      if ( n != child )
      {
         throw E57_EXCEPTION1( ErrorInvarianceViolation );
      }
   }
}

/*!
@class e57::VectorNode

@brief An E57 element containing ordered vector of child nodes.

@details
A VectorNode is a container of ordered child nodes. The child nodes are automatically assigned an
elementName, which is a string version of the positional index of the child starting at "0". Child
nodes may only be appended onto the end of a VectorNode.

A VectorNode that is created with a restriction that its children must have the same type is called
a "homogeneous VectorNode". A VectorNode without such a restriction is called a "heterogeneous
VectorNode".

See Node class discussion for discussion of the common functions that StructureNode supports.

@section vectornode_invariant Class Invariant
A class invariant is a list of statements about an object that are always true before and after any
operation on the object. An invariant is useful for testing correct operation of an implementation.
Statements in an invariant can involve only externally visible state, or can refer to internal
implementation-specific state that is not visible to the API user. The following C++ code checks
externally visible state for consistency and throws an exception if the invariant is violated:

@dontinclude VectorNode.cpp
@skip void VectorNode::checkInvariant
@until ^}

@see Node
*/

/*!
@brief Create a new empty Vector node.

@param [in] destImageFile The ImageFile where the new node will eventually be stored.
@param [in] allowHeteroChildren Will child elements of differing types be allowed in this
VectorNode.

@details
A VectorNode is a ordered container of E57 nodes.

The @a destImageFile indicates which ImageFile the VectorNode will eventually be attached to. A node
is attached to an ImageFile by adding it underneath the predefined root of the ImageFile (gotten
from ImageFile::root). It is not an error to fail to attach the VectorNode to the @a destImageFile.
It is an error to attempt to attach the VectorNode to a different ImageFile.

If @a allowHeteroChildren is false, then the children that are appended to the VectorNode must be
identical in every visible characteristic except the stored values. These visible characteristics
include number of children (for StructureNode and VectorNode descendents), number of
records/prototypes/codecs (for CompressedVectorNode), minimum/maximum attributes (for IntegerNode,
ScaledIntegerNode, FloatNode), byteCount (for BlobNode), scale/offset (for ScaledIntegerNode), and
all elementNames. The enforcement of this homogeneity rule begins when the second child is appended
to the VectorNode, thus it is not an error to modify a child of a homogeneous VectorNode containing
only one child.

If @a allowHeteroChildren is true, then the types of the children of the VectorNode are completely
unconstrained.

@pre The @a destImageFile must be open (i.e. destImageFile.isOpen() must be true).
@pre The @a destImageFile must have been opened in write mode (i.e. destImageFile.isWritable() must
be true).

@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see Node, VectorNode::allowHeteroChildren, ::ErrorHomogeneousViolation
*/
VectorNode::VectorNode( const ImageFile &destImageFile, bool allowHeteroChildren ) :
   impl_( new VectorNodeImpl( destImageFile.impl(), allowHeteroChildren ) )
{
}

/*!
@brief Is this a root node.
@copydetails Node::isRoot()
*/
bool VectorNode::isRoot() const
{
   return impl_->isRoot();
}

/*!
@brief Return parent of node, or self if a root node.
@copydetails Node::parent()
*/
Node VectorNode::parent() const
{
   return Node( impl_->parent() );
}

/*!
@brief Get absolute pathname of node.
@copydetails Node::pathName()
*/
ustring VectorNode::pathName() const
{
   return impl_->pathName();
}

/*!
@brief Get elementName string, that identifies the node in its parent.
@copydetails Node::elementName()
*/
ustring VectorNode::elementName() const
{
   return impl_->elementName();
}

/*!
@brief Get the ImageFile that was declared as the destination for the node when it was created.
@copydetails Node::destImageFile()
*/
ImageFile VectorNode::destImageFile() const
{
   return ImageFile( impl_->destImageFile() );
}

/*!
@brief Has node been attached into the tree of an ImageFile.
@copydetails Node::isAttached()
*/
bool VectorNode::isAttached() const
{
   return impl_->isAttached();
}

/*!
@brief Get whether child elements are allowed to be different types

@details
See the class discussion at bottom of VectorNode page for details of homogeneous/heterogeneous
VectorNode. The returned attribute is determined when the VectorNode is created, and cannot be
changed.

@pre The destination ImageFile must be open (i.e. destImageFile().isOpen()).
@post No visible state is modified.

@return True if child elements can be different types.

@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see ::ErrorHomogeneousViolation
*/
bool VectorNode::allowHeteroChildren() const
{
   return impl_->allowHeteroChildren();
}

/*!
@brief Get number of child elements in this VectorNode.

@pre The destination ImageFile must be open (i.e. destImageFile().isOpen()).
@post No visible state is modified.

@return Number of child elements in this VectorNode.

@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see VectorNode::get(int64_t), VectorNode::append, StructureNode::childCount
*/
int64_t VectorNode::childCount() const
{
   return impl_->childCount();
}

/*!
@brief Is the given pathName defined relative to this node.

@param [in] pathName The absolute pathname, or pathname relative to this object, to check.

@details
The @a pathName may be relative to this node, or absolute (starting with a "/"). The origin of the
absolute path name is the root of the tree that contains this VectorNode. If this VectorNode is not
attached to an ImageFile, the @a pathName origin root will not the root node of an ImageFile.

The element names of child elements of VectorNodes are numbers, encoded as strings, starting at "0".

@pre The destination ImageFile must be open (i.e. destImageFile().isOpen()).
@post No visible state is modified.

@return true if pathName is currently defined.

@throw ::ErrorBadPathName
@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see StructureNode::isDefined
*/
bool VectorNode::isDefined( const ustring &pathName ) const
{
   return impl_->isDefined( pathName );
}

/*!
@brief Get a child element by positional index.

@param [in] index The index of child element to get, starting at 0.

@pre The destination ImageFile must be open (i.e. destImageFile().isOpen()).
@pre 0 <= @a index < childCount()
@post No visible state is modified.

@return A smart Node handle referencing the child node.

@throw ::ErrorChildIndexOutOfBounds
@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see VectorNode::childCount, VectorNode::append, StructureNode::get(int64_t) const
*/
Node VectorNode::get( int64_t index ) const
{
   return Node( impl_->get( index ) );
}

/*!
@brief Get a child element by string path name

@param [in] pathName The pathname, either absolute or relative, of the object to get.

@details
The @a pathName may be relative to this node, or absolute (starting with a "/"). The origin of the
absolute path name is the root of the tree that contains this VectorNode. If this VectorNode is not
attached to an ImageFile, the @a pathName origin root will not the root node of an ImageFile.

The element names of child elements of VectorNodes are numbers, encoded as strings, starting at "0".

@pre The destination ImageFile must be open (i.e. destImageFile().isOpen()).
@pre The @a pathName must be defined (i.e. isDefined(pathName)).
@post No visible state is modified.

@return A smart Node handle referencing the child node.

@throw ::ErrorBadPathName
@throw ::ErrorPathUndefined
@throw ::ErrorImageFileNotOpen
@throw ::ErrorInternal All objects in undocumented state

@see VectorNode::childCount, VectorNode::append, StructureNode::get(int64_t) const
*/
Node VectorNode::get( const ustring &pathName ) const
{
   return Node( impl_->get( pathName ) );
}

/*!
@brief Append a child element to end of VectorNode.

@param [in] n The node to be added as a child at end of the VectorNode.

@details
If the VectorNode is homogeneous and already has at least one child, then @a n must be identical to
the existing children in every visible characteristic except the stored values. These visible
characteristics include number of children (for StructureNode and VectorNode descendents), number of
records/prototypes/codecs (for CompressedVectorNode), minimum/maximum attributes (for IntegerNode,
ScaledIntegerNode, FloatNode), byteCount (for BlobNode), scale/offset (for ScaledIntegerNode), and
all elementNames.

The VectorNode must not be a descendent of a homogeneous VectorNode with more than one child.

@pre The new child node @a n must be a root node (not already having a parent).
@pre The destination ImageFile must be open (i.e. destImageFile().isOpen()).
@pre The associated destImageFile must have been opened in write mode (i.e.
destImageFile().isWritable()).
@post the childCount is incremented.

@throw ::ErrorImageFileNotOpen
@throw ::ErrorHomogeneousViolation
@throw ::ErrorFileReadOnly
@throw ::ErrorAlreadyHasParent
@throw ::ErrorDifferentDestImageFile
@throw ::ErrorInternal All objects in undocumented state

@see VectorNode::childCount, VectorNode::get(int64_t), StructureNode::set
*/
void VectorNode::append( const Node &n )
{
   impl_->append( n.impl() );
}

/*!
@brief Diagnostic function to print internal state of object to output stream in an indented format.
@copydetails Node::dump()
*/
#ifdef E57_DEBUG
void VectorNode::dump( int indent, std::ostream &os ) const
{
   impl_->dump( indent, os );
}
#else
void VectorNode::dump( int indent, std::ostream &os ) const
{
   UNUSED( indent );
   UNUSED( os );
}
#endif

/*!
@brief Upcast a VectorNode handle to a generic Node handle.

@details
An upcast is always safe, and the compiler can automatically insert it for initializations of Node
variables and Node function arguments.

@return A smart Node handle referencing the underlying object.

@throw No E57Exceptions.

@see explanation in Node, Node::type(), VectorNode(const Node&)
*/
VectorNode::operator Node() const
{
   // Implicitly upcast from shared_ptr<VectorNodeImpl> to SharedNodeImplPtr and construct a Node
   // object
   return Node( impl_ );
}

/*!
@brief Downcast a generic Node handle to a VectorNode handle.

@param [in] n The generic handle to downcast.

@details
The handle @a n must be for an underlying VectorNode, otherwise an exception is thrown. In designs
that need to avoid the exception, use Node::type() to determine the actual type of the @a n before
downcasting. This function must be explicitly called (c++ compiler cannot insert it automatically).

@throw ::ErrorBadNodeDowncast

@see Node::type(), VectorNode::operator Node()
*/
VectorNode::VectorNode( const Node &n )
{
   if ( n.type() != TypeVector )
   {
      throw E57_EXCEPTION2( ErrorBadNodeDowncast, "nodeType=" + toString( n.type() ) );
   }

   // Set our shared_ptr to the downcast shared_ptr
   impl_ = std::static_pointer_cast<VectorNodeImpl>( n.impl() );
}

/// @cond documentNonPublic The following isn't part of the API, and isn't documented.
VectorNode::VectorNode( std::shared_ptr<VectorNodeImpl> ni ) : impl_( ni )
{
}
/// @endcond
