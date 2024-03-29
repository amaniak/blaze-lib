//=================================================================================================
/*!
//  \file blaze/math/expressions/TDMatSVecMultExpr.h
//  \brief Header file for the transpose dense matrix/sparse vector multiplication expression
//
//  Copyright (C) 2013 Klaus Iglberger - All Rights Reserved
//
//  This file is part of the Blaze library. You can redistribute it and/or modify it under
//  the terms of the New (Revised) BSD License. Redistribution and use in source and binary
//  forms, with or without modification, are permitted provided that the following conditions
//  are met:
//
//  1. Redistributions of source code must retain the above copyright notice, this list of
//     conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice, this list
//     of conditions and the following disclaimer in the documentation and/or other materials
//     provided with the distribution.
//  3. Neither the names of the Blaze development group nor the names of its contributors
//     may be used to endorse or promote products derived from this software without specific
//     prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
//  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
//  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
//  SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
//  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
//  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
//  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
//  DAMAGE.
*/
//=================================================================================================

#ifndef _BLAZE_MATH_EXPRESSIONS_TDMATSVECMULTEXPR_H_
#define _BLAZE_MATH_EXPRESSIONS_TDMATSVECMULTEXPR_H_


//*************************************************************************************************
// Includes
//*************************************************************************************************

#include <stdexcept>
#include <blaze/math/constraints/DenseMatrix.h>
#include <blaze/math/constraints/DenseVector.h>
#include <blaze/math/constraints/MatVecMultExpr.h>
#include <blaze/math/constraints/SparseVector.h>
#include <blaze/math/constraints/StorageOrder.h>
#include <blaze/math/constraints/TransposeFlag.h>
#include <blaze/math/expressions/Computation.h>
#include <blaze/math/expressions/DenseVector.h>
#include <blaze/math/expressions/Forward.h>
#include <blaze/math/expressions/MatVecMultExpr.h>
#include <blaze/math/Intrinsics.h>
#include <blaze/math/shims/Reset.h>
#include <blaze/math/shims/Serial.h>
#include <blaze/math/traits/MultExprTrait.h>
#include <blaze/math/traits/MultTrait.h>
#include <blaze/math/traits/SubmatrixExprTrait.h>
#include <blaze/math/traits/SubvectorExprTrait.h>
#include <blaze/math/typetraits/IsComputation.h>
#include <blaze/math/typetraits/IsDiagonal.h>
#include <blaze/math/typetraits/IsExpression.h>
#include <blaze/math/typetraits/IsLower.h>
#include <blaze/math/typetraits/IsMatMatMultExpr.h>
#include <blaze/math/typetraits/IsResizable.h>
#include <blaze/math/typetraits/IsStrictlyLower.h>
#include <blaze/math/typetraits/IsStrictlyUpper.h>
#include <blaze/math/typetraits/IsUpper.h>
#include <blaze/math/typetraits/RequiresEvaluation.h>
#include <blaze/math/typetraits/Rows.h>
#include <blaze/math/typetraits/Size.h>
#include <blaze/system/Thresholds.h>
#include <blaze/util/Assert.h>
#include <blaze/util/constraints/Reference.h>
#include <blaze/util/DisableIf.h>
#include <blaze/util/EnableIf.h>
#include <blaze/util/logging/FunctionTrace.h>
#include <blaze/util/SelectType.h>
#include <blaze/util/Types.h>
#include <blaze/util/typetraits/IsSame.h>
#include <blaze/util/typetraits/RemoveReference.h>


namespace blaze {

//=================================================================================================
//
//  CLASS TDMATSVECMULTEXPR
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Expression object for transpose dense matrix-sparse vector multiplications.
// \ingroup dense_vector_expression
//
// The TDMatSVecMultExpr class represents the compile time expression for multiplications
// between column-major dense matrices and sparse vectors.
*/
template< typename MT    // Type of the left-hand side dense matrix
        , typename VT >  // Type of the right-hand side sparse vector
class TDMatSVecMultExpr : public DenseVector< TDMatSVecMultExpr<MT,VT>, false >
                        , private MatVecMultExpr
                        , private Computation
{
 private:
   //**Type definitions****************************************************************************
   typedef typename MT::ResultType     MRT;  //!< Result type of the left-hand side dense matrix expression.
   typedef typename VT::ResultType     VRT;  //!< Result type of the right-hand side sparse vector expression.
   typedef typename MRT::ElementType   MET;  //!< Element type of the left-hand side dense matrix expression.
   typedef typename VRT::ElementType   VET;  //!< Element type of the right-hand side sparse vector expression.
   typedef typename MT::CompositeType  MCT;  //!< Composite type of the left-hand side dense matrix expression.
   typedef typename VT::CompositeType  VCT;  //!< Composite type of the right-hand side sparse vector expression.
   //**********************************************************************************************

   //**********************************************************************************************
   //! Compilation switch for the composite type of the left-hand side dense matrix expression.
   enum { evaluateMatrix = ( IsComputation<MT>::value && IsSame<MET,VET>::value &&
                             IsBlasCompatible<MET>::value ) || RequiresEvaluation<MT>::value };
   //**********************************************************************************************

   //**********************************************************************************************
   //! Compilation switch for the composite type of the right-hand side dense vector expression.
   enum { evaluateVector = IsComputation<VT>::value || RequiresEvaluation<VT>::value };
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! The UseSMPAssign struct is a helper struct for the selection of the parallel evaluation
       strategy. In case either the matrix or the vector operand requires an intermediate
       evaluation, the nested \value will be set to 1, otherwise it will be 0. */
   template< typename T1 >
   struct UseSMPAssign {
      enum { value = ( evaluateMatrix || evaluateVector ) };
   };
   /*! \endcond */
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case the matrix type and the two involved vector types are suited for a vectorized
       computation of the matrix/vector multiplication, the nested \value will be set to 1,
       otherwise it will be 0. */
   template< typename T1, typename T2, typename T3 >
   struct UseVectorizedKernel {
      enum { value = !IsDiagonal<T2>::value &&
                     T1::vectorizable && T2::vectorizable &&
                     IsSame<typename T1::ElementType,typename T2::ElementType>::value &&
                     IsSame<typename T1::ElementType,typename T3::ElementType>::value &&
                     IntrinsicTrait<typename T1::ElementType>::addition &&
                     IntrinsicTrait<typename T1::ElementType>::multiplication };
   };
   /*! \endcond */
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case a vectorized computation of the matrix/vector multiplication is not possible, but
       a loop-unrolled computation is feasible, the nested \value will be set to 1, otherwise it
       will be 0. */
   template< typename T1, typename T2, typename T3 >
   struct UseOptimizedKernel {
      enum { value = !UseVectorizedKernel<T1,T2,T3>::value &&
                     !IsDiagonal<T2>::value &&
                     !IsResizable<typename T1::ElementType>::value &&
                     !IsResizable<VET>::value };
   };
   /*! \endcond */
   //**********************************************************************************************

   //**********************************************************************************************
   /*! \cond BLAZE_INTERNAL */
   //! Helper structure for the explicit application of the SFINAE principle.
   /*! In case neither a vectorized nor optimized computation is possible, the nested \value will
       be set to 1, otherwise it will be 0. */
   template< typename T1, typename T2, typename T3 >
   struct UseDefaultKernel {
      enum { value = !UseVectorizedKernel<T1,T2,T3>::value &&
                     !UseOptimizedKernel<T1,T2,T3>::value };
   };
   /*! \endcond */
   //**********************************************************************************************

 public:
   //**Type definitions****************************************************************************
   typedef TDMatSVecMultExpr<MT,VT>                    This;           //!< Type of this TDMatSVecMultExpr instance.
   typedef typename MultTrait<MRT,VRT>::Type           ResultType;     //!< Result type for expression template evaluations.
   typedef typename ResultType::TransposeType          TransposeType;  //!< Transpose type for expression template evaluations.
   typedef typename ResultType::ElementType            ElementType;    //!< Resulting element type.
   typedef typename IntrinsicTrait<ElementType>::Type  IntrinsicType;  //!< Resulting intrinsic element type.
   typedef const ElementType                           ReturnType;     //!< Return type for expression template evaluations.
   typedef const ResultType                            CompositeType;  //!< Data type for composite expression templates.

   //! Composite type of the left-hand side dense matrix expression.
   typedef typename SelectType< IsExpression<MT>::value, const MT, const MT& >::Type  LeftOperand;

   //! Composite type of the right-hand side dense vector expression.
   typedef typename SelectType< IsExpression<VT>::value, const VT, const VT& >::Type  RightOperand;

   //! Type for the assignment of the left-hand side dense matrix operand.
   typedef typename SelectType< evaluateMatrix, const MRT, MCT >::Type  LT;

   //! Type for the assignment of the right-hand side dense matrix operand.
   typedef typename SelectType< evaluateVector, const VRT, typename VT::CompositeType >::Type  RT;
   //**********************************************************************************************

   //**Compilation flags***************************************************************************
   //! Compilation switch for the expression template evaluation strategy.
   enum { vectorizable = !IsDiagonal<MT>::value &&
                         MT::vectorizable &&
                         IsSame<MET,VET>::value &&
                         IntrinsicTrait<MET>::addition &&
                         IntrinsicTrait<MET>::multiplication };

   //! Compilation switch for the expression template assignment strategy.
   enum { smpAssignable = !evaluateMatrix && MT::smpAssignable &&
                          !evaluateVector && VT::smpAssignable };
   //**********************************************************************************************

   //**Constructor*********************************************************************************
   /*!\brief Constructor for the TDMatSVecMultExpr class.
   //
   // \param mat The left-hand side dense matrix operand of the multiplication expression.
   // \param vec The right-hand side sparse vector operand of the multiplication expression.
   */
   explicit inline TDMatSVecMultExpr( const MT& mat, const VT& vec )
      : mat_( mat )  // Left-hand side dense matrix of the multiplication expression
      , vec_( vec )  // Right-hand side sparse vector of the multiplication expression
   {
      BLAZE_INTERNAL_ASSERT( mat_.columns() == vec_.size(), "Invalid matrix and vector sizes" );
   }
   //**********************************************************************************************

   //**Subscript operator**************************************************************************
   /*!\brief Subscript operator for the direct access to the vector elements.
   //
   // \param index Access index. The index has to be in the range \f$[0..N-1]\f$.
   // \return The resulting value.
   */
   inline ReturnType operator[]( size_t index ) const {
      BLAZE_INTERNAL_ASSERT( index < mat_.rows(), "Invalid vector access index" );

      typedef typename RemoveReference<VCT>::Type::ConstIterator  ConstIterator;

      VCT x( vec_ );  // Evaluation of the right-hand side sparse vector operand

      BLAZE_INTERNAL_ASSERT( x.size() == vec_.size(), "Invalid vector size" );

      const ConstIterator end( ( IsLower<MT>::value )
                                 ?( IsStrictlyLower<MT>::value ? x.lowerBound( index ) : x.upperBound( index ) )
                                 :( x.end() ) );
      ConstIterator element( ( IsUpper<MT>::value )
                             ?( IsStrictlyUpper<MT>::value ? x.upperBound( index ) : x.lowerBound( index ) )
                             :( x.begin() ) );

      ElementType res = ElementType();

      if( element != end ) {
         res = mat_( index, element->index() ) * element->value();
         ++element;
         for( ; element!=x.end(); ++element ) {
            res += mat_( index, element->index() ) * element->value();
         }
      }

      return res;
   }
   //**********************************************************************************************

   //**Size function*******************************************************************************
   /*!\brief Returns the current size/dimension of the vector.
   //
   // \return The size of the vector.
   */
   inline size_t size() const {
      return mat_.rows();
   }
   //**********************************************************************************************

   //**Left operand access*************************************************************************
   /*!\brief Returns the left-hand side transpose dense matrix operand.
   //
   // \return The left-hand side transpose dense matrix operand.
   */
   inline LeftOperand leftOperand() const {
      return mat_;
   }
   //**********************************************************************************************

   //**Right operand access************************************************************************
   /*!\brief Returns the right-hand side sparse vector operand.
   //
   // \return The right-hand side sparse vector operand.
   */
   inline RightOperand rightOperand() const {
      return vec_;
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression can alias with the given address \a alias.
   //
   // \param alias The alias to be checked.
   // \return \a true in case the expression can alias, \a false otherwise.
   */
   template< typename T >
   inline bool canAlias( const T* alias ) const {
      return mat_.isAliased( alias ) || vec_.isAliased( alias );
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression is aliased with the given address \a alias.
   //
   // \param alias The alias to be checked.
   // \return \a true in case an alias effect is detected, \a false otherwise.
   */
   template< typename T >
   inline bool isAliased( const T* alias ) const {
      return mat_.isAliased( alias ) || vec_.isAliased( alias );
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the operands of the expression are properly aligned in memory.
   //
   // \return \a true in case the operands are aligned, \a false if not.
   */
   inline bool isAligned() const {
      return mat_.isAligned();
   }
   //**********************************************************************************************

   //**********************************************************************************************
   /*!\brief Returns whether the expression can be used in SMP assignments.
   //
   // \return \a true in case the expression can be used in SMP assignments, \a false if not.
   */
   inline bool canSMPAssign() const {
      return ( size() > SMP_TDMATSVECMULT_THRESHOLD );
   }
   //**********************************************************************************************

 private:
   //**Member variables****************************************************************************
   LeftOperand  mat_;  //!< Left-hand side dense matrix of the multiplication expression.
   RightOperand vec_;  //!< Right-hand side sparse vector of the multiplication expression.
   //**********************************************************************************************

   //**Assignment to dense vectors*****************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Assignment of a transpose dense matrix-sparse vector multiplication to a dense vector
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized assignment of a transpose dense matrix-
   // sparse vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void assign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      // Evaluation of the right-hand side sparse vector operand
      RT x( serial( rhs.vec_ ) );
      if( x.nonZeros() == 0UL ) {
         reset( ~lhs );
         return;
      }

      // Evaluation of the left-hand side dense matrix operand
      LT A( serial( rhs.mat_ ) );

      // Checking the evaluated operands
      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      // Performing the dense matrix-sparse vector multiplication
      TDMatSVecMultExpr::selectAssignKernel( ~lhs, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default assignment to dense vectors*********************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default assignment of a transpose dense matrix-sparse vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the default assignment kernel for the transpose dense matrix-
   // sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseDefaultKernel<VT1,MT1,VT2> >::Type
      selectAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      size_t last( 0UL );

      if( IsLower<MT1>::value ) {
         const size_t iend( IsStrictlyLower<MT1>::value ? element->index()+1UL : element->index() );
         for( size_t i=0UL; i<iend; ++i )
            reset( y[i] );
      }

      for( ; element!=end; ++element )
      {
         const size_t index( element->index() );

         if( IsDiagonal<MT1>::value )
         {
            for( size_t i=last; i<index; ++i )
               reset( y[i] );

            y[index] = A(index,index) * element->value();
            last = index + 1UL;
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? index+1UL : index )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? index : index+1UL )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            for( size_t i=ibegin; i<last; ++i ) {
               y[i] += A(i,index) * element->value();
            }
            for( size_t i=last; i<iend; ++i ) {
               y[i] = A(i,index) * element->value();
            }

            last = iend;
         }
      }

      if( IsUpper<MT1>::value ) {
         for( size_t i=last; i<M; ++i )
            reset( y[i] );
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Optimized assignment to dense vectors*******************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Optimized assignment of a transpose dense matrix-sparse vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the optimized assignment kernel for the transpose dense matrix-
   // sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseOptimizedKernel<VT1,MT1,VT2> >::Type
      selectAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      const size_t jpos( x.nonZeros() & size_t(-4) );
      BLAZE_INTERNAL_ASSERT( ( x.nonZeros() - ( x.nonZeros() % 4UL ) ) == jpos, "Invalid end calculation" );

      if( jpos > 3UL )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );
         ++element;
         const size_t j2( element->index() );
         const VET    v2( element->value() );
         ++element;
         const size_t j3( element->index() );
         const VET    v3( element->value() );
         ++element;
         const size_t j4( element->index() );
         const VET    v4( element->value() );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         for( size_t i=0UL; i<M; ++i ) {
            y[i] = A(i,j1) * v1 + A(i,j2) * v2 + A(i,j3) * v3 + A(i,j4) * v4;
         }
      }
      else
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );
         ++element;

         for( size_t i=0UL; i<M; ++i ) {
            y[i] = A(i,j1) * v1;
         }
      }

      for( size_t j=(jpos>3UL)?(4UL):(1UL); (j+4UL)<=jpos; j+=4UL )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );
         ++element;
         const size_t j2( element->index() );
         const VET    v2( element->value() );
         ++element;
         const size_t j3( element->index() );
         const VET    v3( element->value() );
         ++element;
         const size_t j4( element->index() );
         const VET    v4( element->value() );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( IsStrictlyLower<MT1>::value ? j1+1UL : j1 )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j4 : j4+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; ++i ) {
            y[i] += A(i,j1) * v1 + A(i,j2) * v2 + A(i,j3) * v3 + A(i,j4) * v4;
         }
      }
      for( ; element!=end; ++element )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( IsStrictlyLower<MT1>::value ? j1+1UL : j1 )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j1 : j1+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; ++i ) {
            y[i] += A(i,j1) * v1;
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized assignment to dense vectors******************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized assignment of a transpose dense matrix-sparse vector multiplication
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the vectorized assignment kernel for the transpose dense matrix-
   // sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseVectorizedKernel<VT1,MT1,VT2> >::Type
      selectAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef IntrinsicTrait<ElementType>  IT;
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      const size_t jpos( x.nonZeros() & size_t(-4) );
      BLAZE_INTERNAL_ASSERT( ( x.nonZeros() - ( x.nonZeros() % 4UL ) ) == jpos, "Invalid end calculation" );

      if( jpos > 3UL )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );
         ++element;
         const size_t        j2( element->index() );
         const IntrinsicType v2( set( element->value() ) );
         ++element;
         const size_t        j3( element->index() );
         const IntrinsicType v3( set( element->value() ) );
         ++element;
         const size_t        j4( element->index() );
         const IntrinsicType v4( set( element->value() ) );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         for( size_t i=0UL; i<M; i+=IT::size ) {
            y.store( i, A.load(i,j1) * v1 + A.load(i,j2) * v2 + A.load(i,j3) * v3 + A.load(i,j4) * v4 );
         }
      }
      else
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );
         ++element;

         for( size_t i=0UL; i<M; i+=IT::size ) {
            y.store( i, A.load(i,j1) * v1 );
         }
      }

      for( size_t j=(jpos>3UL)?(4UL):(1UL); (j+4UL)<=jpos; j+=4UL )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );
         ++element;
         const size_t        j2( element->index() );
         const IntrinsicType v2( set( element->value() ) );
         ++element;
         const size_t        j3( element->index() );
         const IntrinsicType v3( set( element->value() ) );
         ++element;
         const size_t        j4( element->index() );
         const IntrinsicType v4( set( element->value() ) );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( ( IsStrictlyLower<MT1>::value ? j1+1UL : j1 ) & size_t(-IT::size) )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j4 : j4+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; i+=IT::size ) {
            y.store( i, y.load(i) + A.load(i,j1) * v1 + A.load(i,j2) * v2 + A.load(i,j3) * v3 + A.load(i,j4) * v4 );
         }
      }
      for( ; element!=end; ++element )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( ( IsStrictlyLower<MT1>::value ? j1+1UL : j1 ) & size_t(-IT::size) )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j1 : j1+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; i+=IT::size ) {
            y.store( i, y.load(i) + A.load(i,j1) * v1 );
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Assignment to sparse vectors****************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Assignment of a transpose dense matrix-sparse vector multiplication to a sparse vector
   //        (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side sparse vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized assignment of a transpose dense matrix-
   // sparse vector multiplication expression to a sparse vector.
   */
   template< typename VT1 >  // Type of the target sparse vector
   friend inline void assign( SparseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE ( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_REFERENCE_TYPE( typename ResultType::CompositeType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      assign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Addition assignment to dense vectors********************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Addition assignment of a transpose dense matrix-sparse vector multiplication to a
   //        dense vector (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be added.
   // \return void
   //
   // This function implements the performance optimized addition assignment of a transpose dense
   // matrix-sparse vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void addAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      // Evaluation of the right-hand side sparse vector operand
      RT x( serial( rhs.vec_ ) );
      if( x.nonZeros() == 0UL ) return;

      // Evaluation of the left-hand side dense matrix operand
      LT A( serial( rhs.mat_ ) );

      // Checking the evaluated operands
      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      // Performing the dense matrix-sparse vector multiplication
      TDMatSVecMultExpr::selectAddAssignKernel( ~lhs, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default addition assignment to dense vectors************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default addition assignment of a transpose dense matrix-sparse vector multiplication
   //        (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the default addition assignment kernel for the transpose dense
   // matrix-sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseDefaultKernel<VT1,MT1,VT2> >::Type
      selectAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      for( ; element!=end; ++element )
      {
         const size_t index( element->index() );

         if( IsDiagonal<MT1>::value )
         {
            y[index] += A(index,index) * element->value();
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? index+1UL : index )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? index : index+1UL )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            for( size_t i=ibegin; i<iend; ++i ) {
               y[i] += A(i,index) * element->value();
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Optimized addition assignment to dense vectors**********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Optimized addition assignment of a transpose dense matrix-sparse vector multiplication
   //        (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the optimized addition assignment kernel for the transpose dense
   // matrix-sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseOptimizedKernel<VT1,MT1,VT2> >::Type
      selectAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      const size_t jpos( x.nonZeros() & size_t(-4) );
      BLAZE_INTERNAL_ASSERT( ( x.nonZeros() - ( x.nonZeros() % 4UL ) ) == jpos, "Invalid end calculation" );

      for( size_t j=0UL; (j+4UL)<=jpos; j+=4UL )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );
         ++element;
         const size_t j2( element->index() );
         const VET    v2( element->value() );
         ++element;
         const size_t j3( element->index() );
         const VET    v3( element->value() );
         ++element;
         const size_t j4( element->index() );
         const VET    v4( element->value() );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( IsStrictlyLower<MT1>::value ? j1+1UL : j1 )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j4 : j4+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; ++i ) {
            y[i] += A(i,j1) * v1 + A(i,j2) * v2 + A(i,j3) * v3 + A(i,j4) * v4;
         }
      }
      for( ; element!=end; ++element )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( IsStrictlyLower<MT1>::value ? j1+1UL : j1 )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j1 : j1+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; ++i ) {
            y[i] += A(i,j1) * v1;
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized addition assignment to dense vectors*********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized addition assignment of a transpose dense matrix-sparse vector
   //        multiplication (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the vectorized addition assignment kernel for the transpose dense
   // matrix-sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseVectorizedKernel<VT1,MT1,VT2> >::Type
      selectAddAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef IntrinsicTrait<ElementType>  IT;
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      const size_t jpos( x.nonZeros() & size_t(-4) );
      BLAZE_INTERNAL_ASSERT( ( x.nonZeros() - ( x.nonZeros() % 4UL ) ) == jpos, "Invalid end calculation" );

      for( size_t j=0UL; (j+4UL)<=jpos; j+=4UL )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );
         ++element;
         const size_t        j2( element->index() );
         const IntrinsicType v2( set( element->value() ) );
         ++element;
         const size_t        j3( element->index() );
         const IntrinsicType v3( set( element->value() ) );
         ++element;
         const size_t        j4( element->index() );
         const IntrinsicType v4( set( element->value() ) );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( ( IsStrictlyLower<MT1>::value ? j1+1UL : j1 ) & size_t(-IT::size) )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j4 : j4+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; i+=IT::size ) {
            y.store( i, y.load(i) + A.load(i,j1) * v1 + A.load(i,j2) * v2 + A.load(i,j3) * v3 + A.load(i,j4) * v4 );
         }
      }
      for( ; element!=end; ++element )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( ( IsStrictlyLower<MT1>::value ? j1+1UL : j1 ) & size_t(-IT::size) )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j1 : j1+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; i+=IT::size ) {
            y.store( i, y.load(i) + A.load(i,j1) * v1 );
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Addition assignment to sparse vectors*******************************************************
   // No special implementation for the addition assignment to sparse vectors.
   //**********************************************************************************************

   //**Subtraction assignment to dense vectors*****************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Subtraction assignment of a transpose dense matrix-sparse vector multiplication to
   //        a dense vector (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be subtracted.
   // \return void
   //
   // This function implements the performance optimized subtraction assignment of a transpose
   // dense matrix-sparse vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void subAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      // Evaluation of the right-hand side sparse vector operand
      RT x( serial( rhs.vec_ ) );
      if( x.nonZeros() == 0UL ) return;

      // Evaluation of the left-hand side dense matrix operand
      LT A( serial( rhs.mat_ ) );

      // Checking the evaluated operands
      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      // Performing the dense matrix-sparse vector multiplication
      TDMatSVecMultExpr::selectSubAssignKernel( ~lhs, A, x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Default subtraction assignment to dense vectors*********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Default subtraction assignment of a transpose dense matrix-sparse vector
   //        multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the default subtraction assignment kernel for the transpose dense
   // matrix-sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseDefaultKernel<VT1,MT1,VT2> >::Type
      selectSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      for( ; element!=end; ++element )
      {
         const size_t index( element->index() );

         if( IsDiagonal<MT1>::value )
         {
            y[index] -= A(index,index) * element->value();
         }
         else
         {
            const size_t ibegin( ( IsLower<MT1>::value )
                                 ?( IsStrictlyLower<MT1>::value ? index+1UL : index )
                                 :( 0UL ) );
            const size_t iend( ( IsUpper<MT1>::value )
                               ?( IsStrictlyUpper<MT1>::value ? index : index+1UL )
                               :( M ) );
            BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

            for( size_t i=ibegin; i<iend; ++i ) {
               y[i] -= A(i,index) * element->value();
            }
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Optimized subtraction assignment to dense vectors*******************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Optimized subtraction assignment of a transpose dense matrix-sparse vector
   //        multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the optimized subtraction assignment kernel for the transpose dense
   // matrix-sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseOptimizedKernel<VT1,MT1,VT2> >::Type
      selectSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      const size_t jpos( x.nonZeros() & size_t(-4) );
      BLAZE_INTERNAL_ASSERT( ( x.nonZeros() - ( x.nonZeros() % 4UL ) ) == jpos, "Invalid end calculation" );

      for( size_t j=0UL; (j+4UL)<=jpos; j+=4UL )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );
         ++element;
         const size_t j2( element->index() );
         const VET    v2( element->value() );
         ++element;
         const size_t j3( element->index() );
         const VET    v3( element->value() );
         ++element;
         const size_t j4( element->index() );
         const VET    v4( element->value() );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( IsStrictlyLower<MT1>::value ? j1+1UL : j1 )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j4 : j4+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; ++i ) {
            y[i] -= A(i,j1) * v1 + A(i,j2) * v2 + A(i,j3) * v3 + A(i,j4) * v4;
         }
      }
      for( ; element!=end; ++element )
      {
         const size_t j1( element->index() );
         const VET    v1( element->value() );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( IsStrictlyLower<MT1>::value ? j1+1UL : j1 )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j1 : j1+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; ++i ) {
            y[i] -= A(i,j1) * v1;
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Vectorized subtraction assignment to dense vectors******************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Vectorized subtraction assignment of a transpose dense matrix-sparse vector
   //        multiplication (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param y The target left-hand side dense vector.
   // \param A The left-hand side dense matrix operand.
   // \param x The right-hand side sparse vector operand.
   // \return void
   //
   // This function implements the vectorized subtraction assignment kernel for the transpose
   // dense matrix-sparse vector multiplication.
   */
   template< typename VT1    // Type of the left-hand side target vector
           , typename MT1    // Type of the left-hand side matrix operand
           , typename VT2 >  // Type of the right-hand side vector operand
   static inline typename EnableIf< UseVectorizedKernel<VT1,MT1,VT2> >::Type
      selectSubAssignKernel( VT1& y, const MT1& A, const VT2& x )
   {
      typedef IntrinsicTrait<ElementType>  IT;
      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      BLAZE_INTERNAL_ASSERT( x.nonZeros() != 0UL, "Invalid number of non-zero elements" );

      const size_t M( A.rows() );

      ConstIterator element( x.begin() );
      const ConstIterator end( x.end() );

      const size_t jpos( x.nonZeros() & size_t(-4) );
      BLAZE_INTERNAL_ASSERT( ( x.nonZeros() - ( x.nonZeros() % 4UL ) ) == jpos, "Invalid end calculation" );

      for( size_t j=0UL; (j+4UL)<=jpos; j+=4UL )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );
         ++element;
         const size_t        j2( element->index() );
         const IntrinsicType v2( set( element->value() ) );
         ++element;
         const size_t        j3( element->index() );
         const IntrinsicType v3( set( element->value() ) );
         ++element;
         const size_t        j4( element->index() );
         const IntrinsicType v4( set( element->value() ) );
         ++element;

         BLAZE_INTERNAL_ASSERT( j1 < j2 && j2 < j3 && j3 < j4, "Invalid sparse vector index detected" );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( ( IsStrictlyLower<MT1>::value ? j1+1UL : j1 ) & size_t(-IT::size) )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j4 : j4+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; i+=IT::size ) {
            y.store( i, y.load(i) - A.load(i,j1) * v1 - A.load(i,j2) * v2 - A.load(i,j3) * v3 - A.load(i,j4) * v4 );
         }
      }
      for( ; element!=end; ++element )
      {
         const size_t        j1( element->index() );
         const IntrinsicType v1( set( element->value() ) );

         const size_t ibegin( ( IsLower<MT1>::value )
                              ?( ( IsStrictlyLower<MT1>::value ? j1+1UL : j1 ) & size_t(-IT::size) )
                              :( 0UL ) );
         const size_t iend( ( IsUpper<MT1>::value )
                            ?( IsStrictlyUpper<MT1>::value ? j1 : j1+1UL )
                            :( M ) );
         BLAZE_INTERNAL_ASSERT( ibegin <= iend, "Invalid loop indices detected" );

         for( size_t i=ibegin; i<iend; i+=IT::size ) {
            y.store( i, y.load(i) - A.load(i,j1) * v1 );
         }
      }
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Subtraction assignment to sparse vectors****************************************************
   // No special implementation for the subtraction assignment to sparse vectors.
   //**********************************************************************************************

   //**Multiplication assignment to dense vectors**************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief Multiplication assignment of a transpose dense matrix-sparse vector multiplication
   //        to a dense vector (\f$ \vec{y}*=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be multiplied.
   // \return void
   //
   // This function implements the performance optimized multiplication assignment of a transpose
   // dense matrix-sparse vector multiplication expression to a dense vector.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline void multAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE ( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_REFERENCE_TYPE( typename ResultType::CompositeType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( serial( rhs ) );
      multAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**Multiplication assignment to sparse vectors*************************************************
   // No special implementation for the multiplication assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP assignment to dense vectors*****************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP assignment of a transpose dense matrix-sparse vector multiplication to a dense
   //        vector (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized SMP assignment of a transpose dense
   // matrix-sparse vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler
   // in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline typename EnableIf< UseSMPAssign<VT1> >::Type
      smpAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      // Evaluation of the right-hand side sparse vector operand
      RT x( rhs.vec_ );
      if( x.nonZeros() == 0UL ) {
         reset( ~lhs );
         return;
      }

      // Evaluation of the left-hand side dense matrix operand
      LT A( rhs.mat_ );

      // Checking the evaluated operands
      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      // Performing the dense matrix-sparse vector multiplication
      smpAssign( ~lhs, A * x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP assignment to sparse vectors************************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP assignment of a transpose dense matrix-sparse vector multiplication to a sparse
   //        vector (\f$ \vec{y}=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side sparse vector.
   // \param rhs The right-hand side multiplication expression to be assigned.
   // \return void
   //
   // This function implements the performance optimized SMP assignment of a transpose dense
   // matrix-sparse vector multiplication expression to a sparse vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler
   // in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target sparse vector
   friend inline typename EnableIf< UseSMPAssign<VT1> >::Type
      smpAssign( SparseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE ( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_REFERENCE_TYPE( typename ResultType::CompositeType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP addition assignment to dense vectors****************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP addition assignment of a transpose dense matrix-sparse vector multiplication to
   //        a dense vector (\f$ \vec{y}+=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be added.
   // \return void
   //
   // This function implements the performance optimized SMP addition assignment of a transpose
   // dense matrix-sparse vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler
   // in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline typename EnableIf< UseSMPAssign<VT1> >::Type
      smpAddAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      // Evaluation of the right-hand side sparse vector operand
      RT x( rhs.vec_ );
      if( x.nonZeros() == 0UL ) return;

      // Evaluation of the left-hand side dense matrix operand
      LT A( rhs.mat_ );

      // Checking the evaluated operands
      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      // Performing the dense matrix-sparse vector multiplication
      smpAddAssign( ~lhs, A * x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP addition assignment to sparse vectors***************************************************
   // No special implementation for the SMP addition assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP subtraction assignment to dense vectors*************************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP subtraction assignment of a transpose dense matrix-sparse vector multiplication
   //        to a dense vector (\f$ \vec{y}-=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be subtracted.
   // \return void
   //
   // This function implements the performance optimized SMP subtraction assignment of a transpose
   // dense matrix-sparse vector multiplication expression to a dense vector. Due to the explicit
   // application of the SFINAE principle, this function can only be selected by the compiler in
   // case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline typename EnableIf< UseSMPAssign<VT1> >::Type
      smpSubAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      typedef typename RemoveReference<RT>::Type::ConstIterator  ConstIterator;

      // Evaluation of the right-hand side sparse vector operand
      RT x( rhs.vec_ );
      if( x.nonZeros() == 0UL ) return;

      // Evaluation of the left-hand side dense matrix operand
      LT A( rhs.mat_ );

      // Checking the evaluated operands
      BLAZE_INTERNAL_ASSERT( A.rows()    == rhs.mat_.rows()   , "Invalid number of rows"    );
      BLAZE_INTERNAL_ASSERT( A.columns() == rhs.mat_.columns(), "Invalid number of columns" );
      BLAZE_INTERNAL_ASSERT( x.size()    == rhs.vec_.size()   , "Invalid vector size"       );
      BLAZE_INTERNAL_ASSERT( A.rows()    == (~lhs).size()     , "Invalid vector size"       );

      // Performing the dense matrix-sparse vector multiplication
      smpSubAssign( ~lhs, A * x );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP subtraction assignment to sparse vectors************************************************
   // No special implementation for the SMP subtraction assignment to sparse vectors.
   //**********************************************************************************************

   //**SMP multiplication assignment to dense vectors**********************************************
   /*! \cond BLAZE_INTERNAL */
   /*!\brief SMP multiplication assignment of a transpose dense matrix-sparse vector multiplication
   //        to a dense vector (\f$ \vec{y}*=A*\vec{x} \f$).
   // \ingroup dense_vector
   //
   // \param lhs The target left-hand side dense vector.
   // \param rhs The right-hand side multiplication expression to be multiplied.
   // \return void
   //
   // This function implements the performance optimized SMP multiplication assignment of a
   // transpose dense matrix-sparse vector multiplication expression to a dense vector. Due
   // to the explicit application of the SFINAE principle, this function can only be selected
   // by the compiler in case the expression specific parallel evaluation strategy is selected.
   */
   template< typename VT1 >  // Type of the target dense vector
   friend inline typename EnableIf< UseSMPAssign<VT1> >::Type
      smpMultAssign( DenseVector<VT1,false>& lhs, const TDMatSVecMultExpr& rhs )
   {
      BLAZE_FUNCTION_TRACE;

      BLAZE_CONSTRAINT_MUST_BE_DENSE_VECTOR_TYPE ( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( ResultType );
      BLAZE_CONSTRAINT_MUST_BE_REFERENCE_TYPE( typename ResultType::CompositeType );

      BLAZE_INTERNAL_ASSERT( (~lhs).size() == rhs.size(), "Invalid vector sizes" );

      const ResultType tmp( rhs );
      smpMultAssign( ~lhs, tmp );
   }
   /*! \endcond */
   //**********************************************************************************************

   //**SMP multiplication assignment to sparse vectors*********************************************
   // No special implementation for the SMP multiplication assignment to sparse vectors.
   //**********************************************************************************************

   //**Compile time checks*************************************************************************
   /*! \cond BLAZE_INTERNAL */
   BLAZE_CONSTRAINT_MUST_BE_DENSE_MATRIX_TYPE( MT );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_MAJOR_MATRIX_TYPE( MT );
   BLAZE_CONSTRAINT_MUST_BE_SPARSE_VECTOR_TYPE( VT );
   BLAZE_CONSTRAINT_MUST_BE_COLUMN_VECTOR_TYPE( VT );
   BLAZE_CONSTRAINT_MUST_FORM_VALID_MATVECMULTEXPR( MT, VT );
   /*! \endcond */
   //**********************************************************************************************
};
//*************************************************************************************************




//=================================================================================================
//
//  GLOBAL BINARY ARITHMETIC OPERATORS
//
//=================================================================================================

//*************************************************************************************************
/*!\brief Multiplication operator for the multiplication of a column-major dense matrix and a
//        sparse vector (\f$ \vec{y}=A*\vec{x} \f$).
// \ingroup dense_vector
//
// \param mat The left-hand side column-major dense matrix for the multiplication.
// \param vec The right-hand side sparse vector for the multiplication.
// \return The resulting vector.
// \exception std::invalid_argument Matrix and vector sizes do not match.
//
// This operator represents the multiplication between a column-major dense matrix and a sparse
// vector:

   \code
   using blaze::columnMajor;
   using blaze::columnVector;

   blaze::DynamicMatrix<double,columnMajor> A;
   blaze::CompressedVector<double,columnVector> x;
   blaze::DynamicVector<double,columnVector> y;
   // ... Resizing and initialization
   y = A * x;
   \endcode

// The operator returns an expression representing a dense vector of the higher-order element
// type of the two involved element types \a T1::ElementType and \a T2::ElementType. Both the
// dense matrix type \a T1 and the sparse vector type \a T2 as well as the two element types
// \a T1::ElementType and \a T2::ElementType have to be supported by the MultTrait class
// template.\n
// In case the current size of the vector \a vec doesn't match the current number of columns
// of the matrix \a mat, a \a std::invalid_argument is thrown.
*/
template< typename T1    // Type of the left-hand side dense matrix
        , typename T2 >  // Type of the right-hand side sparse vector
inline const typename DisableIf< IsMatMatMultExpr<T1>, TDMatSVecMultExpr<T1,T2> >::Type
   operator*( const DenseMatrix<T1,true>& mat, const SparseVector<T2,false>& vec )
{
   BLAZE_FUNCTION_TRACE;

   if( (~mat).columns() != (~vec).size() )
      throw std::invalid_argument( "Matrix and vector sizes do not match" );

   return TDMatSVecMultExpr<T1,T2>( ~mat, ~vec );
}
//*************************************************************************************************




//=================================================================================================
//
//  SIZE SPECIALIZATIONS
//
//=================================================================================================

//*************************************************************************************************
/*! \cond BLAZE_INTERNAL */
template< typename MT, typename VT >
struct Size< TDMatSVecMultExpr<MT,VT> >
   : public Rows<MT>
{};
/*! \endcond */
//*************************************************************************************************




//=================================================================================================
//
//  EXPRESSION TRAIT SPECIALIZATIONS
//
//=================================================================================================

//*************************************************************************************************
/*! \cond BLAZE_INTERNAL */
template< typename MT, typename VT, bool AF >
struct SubvectorExprTrait< TDMatSVecMultExpr<MT,VT>, AF >
{
 public:
   //**********************************************************************************************
   typedef typename MultExprTrait< typename SubmatrixExprTrait<const MT,AF>::Type
                                 , typename SubvectorExprTrait<const VT,AF>::Type >::Type  Type;
   //**********************************************************************************************
};
/*! \endcond */
//*************************************************************************************************

} // namespace blaze

#endif
