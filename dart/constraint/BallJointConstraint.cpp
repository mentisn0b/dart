/*
 * Copyright (c) 2014, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Jeongseok Lee <jslee02@gmail.com>
 *            C. Karen Liu <karenliu@cc.gatech.edu>
 *
 * Geoorgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */
#include <iostream>
#include "dart/constraint/BallJointConstraint.h"

#include "dart/dynamics/BodyNode.h"
#include "dart/dynamics/Skeleton.h"
#include "dart/lcpsolver/lcp.h"

namespace dart {
namespace constraint {

//==============================================================================
  BallJointConstraint::BallJointConstraint(dynamics::BodyNode* _body,
                  Eigen::Vector3d _jointPos) : JointConstraint(_body),
    mOffset1(_body->getTransform().inverse() * _jointPos),
    mOffset2(_jointPos),
    mViolation(Eigen::Vector3d::Zero()),
    mAppliedImpulseIndex(0)
{
  mDim = 3;

  mOldX[0] = 0.0;
  mOldX[1] = 0.0;
  mOldX[2] = 0.0;

  mJacobian1.resize(3, 6);
  mJacobian2.resize(3, 6);
  Eigen::Matrix3d ssm1 = dart::math::makeSkewSymmetric(-mOffset1);
  mJacobian1.block<3, 3>(0, 0) = ssm1;
  mJacobian1.block<3, 3>(0, 3) = Eigen::MatrixXd::Identity(3, 3);
}

//==============================================================================
BallJointConstraint::BallJointConstraint(dynamics::BodyNode* _body1,
                                         dynamics::BodyNode* _body2,
                                         Eigen::Vector3d _jointPos)
    : JointConstraint(_body1, _body2),
    mOffset1(_body1->getTransform().inverse() * _jointPos),
    mOffset2(_body2->getTransform().inverse() * _jointPos),
    mViolation(Eigen::Vector3d::Zero()),
    mAppliedImpulseIndex(0)
{
  mDim = 3;

  mOldX[0] = 0.0;
  mOldX[1] = 0.0;
  mOldX[2] = 0.0;

  mJacobian1.resize(3, 6);
  mJacobian2.resize(3, 6);
  Eigen::Matrix3d ssm1 = dart::math::makeSkewSymmetric(-mOffset1);
  Eigen::Matrix3d ssm2 = dart::math::makeSkewSymmetric(mBodyNode1->getTransform().inverse() * mBodyNode2->getTransform() * mOffset2);
  mJacobian1.block<3, 3>(0, 0) = ssm1;
  mJacobian1.block<3, 3>(0, 3) = Eigen::MatrixXd::Identity(3, 3);
  mJacobian2.block<3, 3>(0, 0) = ssm2;
  mJacobian2.block<3, 3>(0, 3) = Eigen::MatrixXd::Identity(3, 3);
}

//==============================================================================
BallJointConstraint::~BallJointConstraint()
{
}

//==============================================================================
void BallJointConstraint::update()
{
  // mBodyNode1 should not be null pointer ever
  assert(mBodyNode1);

  if (mBodyNode2)
  {
    Eigen::Vector3d p2 = mBodyNode2->getTransform() * mOffset2;

    mViolation = mOffset1 - mBodyNode1->getTransform().inverse() * mBodyNode2->getTransform() * mOffset2;
  }
  else
  {
    mViolation = mOffset1 - mBodyNode1->getTransform().inverse() * mOffset2;
  }

  //  std::cout << "mViolation = " << mViolation << std::endl;
}

//==============================================================================
void BallJointConstraint::getInformation(ConstraintInfo* _lcp)
{
  assert(_lcp->w[0] == 0.0);
  assert(_lcp->w[1] == 0.0);
  assert(_lcp->w[2] == 0.0);

  assert(_lcp->findex[0] == -1);
  assert(_lcp->findex[1] == -1);
  assert(_lcp->findex[2] == -1);

  _lcp->lo[0] = -dInfinity;
  _lcp->lo[1] = -dInfinity;
  _lcp->lo[2] = -dInfinity;

  _lcp->hi[0] = dInfinity;
  _lcp->hi[1] = dInfinity;
  _lcp->hi[2] = dInfinity;

  _lcp->x[0] = mOldX[0];
  _lcp->x[1] = mOldX[1];
  _lcp->x[2] = mOldX[2];

  Eigen::Vector3d negativeVel = -mBodyNode1->getBodyLinearVelocity(mOffset1);
  if (mBodyNode2)
    negativeVel += mBodyNode1->getTransform().inverse() 
                   * mBodyNode2->getTransform() 
                   * mBodyNode2->getBodyLinearVelocity(mOffset2);

  mViolation *= mErrorReductionParameter * _lcp->invTimeStep;

  // _lcp->b[0] = -mViolation[0];
  // _lcp->b[1] = -mViolation[1];
  // _lcp->b[2] = - mViolation[2];
  _lcp->b[0] = negativeVel[0] - mViolation[0];
  _lcp->b[1] = negativeVel[1] - mViolation[1];
  _lcp->b[2] = negativeVel[2] - mViolation[2];
  //  std::cout << "b: " << _lcp->b[0] << " " << _lcp->b[1] << " " << _lcp->b[2] << " " << std::endl;
}

//==============================================================================
void BallJointConstraint::applyUnitImpulse(size_t _index)
{
  assert(0 <= _index && _index < mDim && "Invalid Index.");
  assert(isActive());

  if (mBodyNode2)
  {
    assert(mBodyNode1->isReactive() || mBodyNode2->isReactive());

    // Self collision case
    if (mBodyNode1->getSkeleton() == mBodyNode2->getSkeleton())
    {
      mBodyNode1->getSkeleton()->clearConstraintImpulses();

      if (mBodyNode1->isReactive())
      {
        if (mBodyNode2->isReactive())
        {
          mBodyNode1->getSkeleton()->updateBiasImpulse(
                 mBodyNode1, mJacobian1.row(_index), 
                 mBodyNode2, mJacobian2.row(_index));
        }
        else
        {
          mBodyNode1->getSkeleton()->updateBiasImpulse(
                 mBodyNode1, mJacobian1.row(_index));
        }
      }
      else
      {
        if (mBodyNode2->isReactive())
        {
          mBodyNode2->getSkeleton()->updateBiasImpulse(
                 mBodyNode2, mJacobian2.row(_index));

        }
        else
        {
          assert(0);
        }
      }
      mBodyNode1->getSkeleton()->updateVelocityChange();
    }
    // Colliding two distinct skeletons
    else
    {
      if (mBodyNode1->isReactive())
      {
        mBodyNode1->getSkeleton()->clearConstraintImpulses();
        mBodyNode1->getSkeleton()->updateBiasImpulse(
              mBodyNode1, mJacobian1.row(_index));
        mBodyNode1->getSkeleton()->updateVelocityChange();
      }

      if (mBodyNode2->isReactive())
      {
        mBodyNode2->getSkeleton()->clearConstraintImpulses();
        mBodyNode2->getSkeleton()->updateBiasImpulse(
              mBodyNode2, mJacobian2.row(_index));
        mBodyNode2->getSkeleton()->updateVelocityChange();
      }
    }
  }
  else
  {
    assert(mBodyNode1->isReactive());

    mBodyNode1->getSkeleton()->clearConstraintImpulses();
    mBodyNode1->getSkeleton()->updateBiasImpulse(
         mBodyNode1, mJacobian1.transpose().col(_index));
    mBodyNode1->getSkeleton()->updateVelocityChange();
  }

  mAppliedImpulseIndex = _index;
}

//==============================================================================
void BallJointConstraint::getVelocityChange(double* _vel, bool _withCfm)
{
  assert(_vel != NULL && "Null pointer is not allowed.");

  for (size_t i = 0; i < mDim; ++i)
   _vel[i] = 0.0;

  if (mBodyNode1->getSkeleton()->isImpulseApplied()
        && mBodyNode1->isReactive())
  {
    Eigen::Vector3d v1 = mJacobian1 * mBodyNode1->getBodyVelocityChange();
    // std::cout << "velChange " << mBodyNode1->getBodyVelocityChange() << std::endl;
    // std::cout << "v1: " << v1 << std::endl;
    for (size_t i = 0; i < mDim; ++i)
      _vel[i] += v1[i];
  }

  if (mBodyNode2 && mBodyNode2->getSkeleton()->isImpulseApplied()
        && mBodyNode2->isReactive())
  {
    Eigen::Vector3d v2 = mJacobian2 * mBodyNode2->getBodyVelocityChange();
    // std::cout << "v2: " << v2 << std::endl;
    for (size_t i = 0; i < mDim; ++i)
      _vel[i] += v2[i];
  }

  // Add small values to diagnal to keep it away from singular, similar to cfm
  // varaible in ODE
  if (_withCfm)
  {
    _vel[mAppliedImpulseIndex] += _vel[mAppliedImpulseIndex]
                                     * mConstraintForceMixing;
  }
}

//==============================================================================
void BallJointConstraint::excite()
{
  if (mBodyNode1->isReactive())
    mBodyNode1->getSkeleton()->setImpulseApplied(true);

  if (mBodyNode2 == NULL)
    return;

  if (mBodyNode2->isReactive())
    mBodyNode2->getSkeleton()->setImpulseApplied(true);
}

//==============================================================================
void BallJointConstraint::unexcite()
{
  if (mBodyNode1->isReactive())
    mBodyNode1->getSkeleton()->setImpulseApplied(false);

  if (mBodyNode2 == NULL)
    return;

  if (mBodyNode2->isReactive())
    mBodyNode2->getSkeleton()->setImpulseApplied(false);
}

//==============================================================================
void BallJointConstraint::applyImpulse(double* _lambda)
{
  mOldX[0] = _lambda[0];
  mOldX[1] = _lambda[1];
  mOldX[2] = _lambda[2];

  Eigen::Vector3d imp;
  imp << _lambda[0], _lambda[1], _lambda[2];

  // std::cout << "lambda: " << _lambda[0] << " " << _lambda[1] << " " << _lambda[2] << std::endl;

  mBodyNode1->addConstraintImpulse(mJacobian1.transpose() * imp);

  if (mBodyNode2)
    mBodyNode2->addConstraintImpulse(mJacobian2.transpose() * imp);

}

//==============================================================================
dynamics::Skeleton* BallJointConstraint::getRootSkeleton() const
{
  if (mBodyNode1->isReactive())
    return mBodyNode1->getSkeleton()->mUnionRootSkeleton;

  if (mBodyNode2)
  {
    if (mBodyNode2->isReactive())
    {
      return mBodyNode2->getSkeleton()->mUnionRootSkeleton;
    }
    else
    {
      assert(0);
      return NULL;
    }
  }
  else
  {
    assert(0);
    return NULL;
  }
}

//==============================================================================
void BallJointConstraint::uniteSkeletons()
{
  if (mBodyNode2 == NULL)
    return;

  if (!mBodyNode1->isReactive() || !mBodyNode2->isReactive())
    return;

  if (mBodyNode1->getSkeleton() == mBodyNode2->getSkeleton())
    return;

  dynamics::Skeleton* unionId1
      = Constraint::compressPath(mBodyNode1->getSkeleton());
  dynamics::Skeleton* unionId2
      = Constraint::compressPath(mBodyNode2->getSkeleton());

  if (unionId1 == unionId2)
    return;

  if (unionId1->mUnionSize < unionId2->mUnionSize)
  {
    // Merge root1 --> root2
    unionId1->mUnionRootSkeleton = unionId2;
    unionId2->mUnionSize += unionId1->mUnionSize;
  }
  else
  {
    // Merge root2 --> root1
    unionId2->mUnionRootSkeleton = unionId1;
    unionId1->mUnionSize += unionId2->mUnionSize;
  }
}

bool BallJointConstraint::isActive() const
{
  if (mBodyNode1->isReactive())
    return true;

  if (mBodyNode2)
  {
    if (mBodyNode2->isReactive())
      return true;
    else
      return false;
  }
  else
  {
    return false;
  }
}

} // namespace constraint
} // namespace dart
