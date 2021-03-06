// Copyright (c) 2016, LAAS-CNRS
// Authors: Joseph Mirabel (joseph.mirabel@laas.fr)
//
// This file is part of hpp-pinocchio.
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
// hpp-pinocchio. If not, see <http://www.gnu.org/licenses/>.

#include "hpp/pinocchio/center-of-mass-computation.hh"

#include <algorithm>
#include <boost/foreach.hpp>

#include <pinocchio/algorithm/center-of-mass.hpp>
#include <pinocchio/algorithm/copy.hpp>

#include <hpp/util/exception-factory.hh>

#include "hpp/pinocchio/joint.hh"
#include "hpp/pinocchio/device.hh"

#include "center-of-mass-computation/algorithm.cc"

namespace hpp {
  namespace pinocchio {
    CenterOfMassComputationPtr_t CenterOfMassComputation::create (
        const DevicePtr_t& d)
    {
      return CenterOfMassComputationPtr_t (new CenterOfMassComputation (d));
    }

    void CenterOfMassComputation::compute (const Device::Computation_t& flag)
    {
      const Model& model = robot_->model();

      bool computeCOM = (flag & Device::COM);
      bool computeJac = (flag & Device::JACOBIAN);
      assert(computeCOM && "This does nothing");
      assert (!(computeJac && !computeCOM)); // JACOBIAN => COM

      // update kinematics
      se3::copy<0>(model,robot_->data(),data);

      data.mass[0] = 0;
      if(computeCOM) data.com[0].setZero();
      if(computeJac) data.Jcom.setZero();
    
      // Propagate COM initialization on all joints. 
      // Could be done only on subtree, with minor gain (possibly loss because of 
      // more difficult branch prediction). I dont recommend to implement it.
      for(JointIndex jid=1;jid<JointIndex(model.joints.size());++jid)
        {
          const double &            mass  = model.inertias[jid].mass ();
          data.mass[jid] = mass;
          data.com [jid] = mass*data.oMi[jid].act (model.inertias[jid].lever());
        }

      // Nullify non-subtree com and mass.
      int root = 0;
      for(JointIndex jid=1; std::size_t(jid)<model.joints.size(); ++jid )
        {
          const JointIndex& rootId = roots_[root];
          if(jid == rootId)
            {
              jid = data.lastChild[rootId];
              root ++;
            }
          else 
            {
              data.mass[jid] = 0;
              data.com [jid] .setZero();
            }
        }

      // Assume root is sorted from smallest id.
      // Nasty cast below, from (u-long) size_t to int.
      for( int root=int(roots_.size()-1); root>=0; --root )
      {
        JointIndex rootId = roots_[root];

        // Backward loop on descendents of joint rootId.
        for( JointIndex jid = data.lastChild[rootId];jid>=rootId;--jid )
          {
            if(computeJac)
              se3::JacobianCenterOfMassBackwardStep
                ::run(model.joints[jid],data.joints[jid],
                      se3::JacobianCenterOfMassBackwardStep::ArgsType(model,data,false));
            else
              {
                assert(computeCOM);
                const JointIndex & parent = model.parents[jid];
                data.com [parent] += data.com [jid];
                data.mass[parent] += data.mass[jid];
              }

            //std::cout << data.oMi [jid] << std::endl;
            //std::cout << data.mass[jid] << std::endl;
            //std::cout << data.com [jid].transpose() << std::endl;
          } // end for jid

        // Backward loop on ancestors of joint rootId
        JointIndex jid = model.parents[rootId]; // loop variable
        rootId = (root>0) ? roots_[root-1] : 0;      // root of previous subtree in roots_
        while (jid>rootId)                           // stop when meeting the next subtree
          {
            const JointIndex & parent = model.parents[jid];
            if(computeJac)
              se3::JacobianCenterOfMassBackwardStep
                ::run(model.joints[jid],data.joints[jid],
                      se3::JacobianCenterOfMassBackwardStep::ArgsType(model,data,false));
            else
              {
                assert(computeCOM);
                data.com [parent] += data.com [jid];
                data.mass[parent] += data.mass[jid];
              }
            jid = parent;
          } // end while

      } // end for root in roots_
      
      if(computeCOM) data.com[0]  /= data.mass[0];
      if(computeJac) data.Jcom    /= data.mass[0];
    }

    CenterOfMassComputation::CenterOfMassComputation (const DevicePtr_t& d) :
      robot_(d), roots_ (), //mass_ (0), jacobianCom_ (3, d->numberDof ())
      data(d->model())
    { assert (d->modelPtr()); }

    void CenterOfMassComputation::add (const JointPtr_t& j)
    {
      JointIndex jid = j->index();
      BOOST_FOREACH( const JointIndex rootId,  roots_ )
        {
          assert (std::size_t(rootId)<robot_->model().joints.size());
          // Assert that the new root is not in already-recorded subtrees.
          if( (jid >= rootId) && (data.lastChild[rootId] >= int(jid)) )
            // We are doing something stupid. Should we throw an error
            // or just return silently ?
            HPP_THROW(std::invalid_argument, "Joint " << j->name()
                << " (" << jid << ") is already in a subtree ["
                << rootId << ", " << data.lastChild[rootId] << "]");
        }

      roots_.push_back(jid);
    }

    CenterOfMassComputation::~CenterOfMassComputation ()
    {}
  }  //  namespace pinocchio
}  //  namespace hpp
