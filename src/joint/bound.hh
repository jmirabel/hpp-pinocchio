//
// Copyright (c) 2016 CNRS
// Author: Joseph Mirabel
//
//
// This file is part of hpp-pinocchio
// hpp-pinocchio is free software: you can redistribute it
// and/or modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation, either version
// 3 of the License, or (at your option) any later version.
//
// hpp-pinocchio is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty
// of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Lesser Public License for more details.  You should have
// received a copy of the GNU Lesser General Public License along with
// hpp-pinocchio  If not, see
// <http://www.gnu.org/licenses/>.

#include <pinocchio/algorithm/joint-configuration.hpp>

#include <hpp/pinocchio/liegroup.hh>

namespace hpp {
  namespace pinocchio {
      struct SetBoundStep : public se3::fusion::JointModelVisitor<SetBoundStep>
      {
        typedef boost::fusion::vector<const Configuration_t &,
                Configuration_t &> ArgsType;

        JOINT_MODEL_VISITOR_INIT(SetBoundStep);

        template<typename JointModel>
        static void algo(const se3::JointModelBase<JointModel> & jmodel,
            const Configuration_t & bounds,
            Configuration_t& out)
        {
          ::hpp::pinocchio::LieGroupTpl::template operation<JointModel>::type
            ::setBound(bounds,
                       jmodel.jointConfigSelector(out));
        }
      };

      template <>
      void SetBoundStep::algo<se3::JointModelComposite>(
          const se3::JointModelBase<se3::JointModelComposite> & jmodel,
          const Configuration_t & bounds,
          Configuration_t & out)
      {
        se3::details::Dispatch<SetBoundStep>::run(
            jmodel.derived(),
            ArgsType(bounds, out));
      }
  } // namespace pinocchio
} // namespace hpp
