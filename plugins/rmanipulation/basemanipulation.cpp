// -*- coding: utf-8 -*-
// Copyright (C) 2006-2010 Rosen Diankov (rdiankov@cs.cmu.edu)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include "commonmanipulation.h"

class BaseManipulation : public ProblemInstance
{
public:
 BaseManipulation(EnvironmentBasePtr penv) : ProblemInstance(penv) {
        __description = ":Interface Author: Rosen Diankov\nVery useful routines for manipulation planning and planning in general. The planners use analytical inverse kinematics and search based techniques.";
        RegisterCommand("SetActiveManip",boost::bind(&BaseManipulation::SetActiveManip,this,_1,_2),
                        "Set the active manipulator");
        RegisterCommand("Traj",boost::bind(&BaseManipulation::Traj,this,_1,_2),
                        "Execute a trajectory from a file on the local filesystem");
        RegisterCommand("GrabBody",boost::bind(&BaseManipulation::GrabBody,this,_1,_2),
                        "Robot calls ::Grab on a body with its current manipulator");
        RegisterCommand("ReleaseAll",boost::bind(&BaseManipulation::ReleaseAll,this,_1,_2),
                        "Releases all grabbed bodies (RobotBase::ReleaseAllGrabbed).");
        RegisterCommand("MoveHandStraight",boost::bind(&BaseManipulation::MoveHandStraight,this,_1,_2),
                        "Move the active end-effector in a straight line until collision or IK fails. Parameters:\n\n\
- stepsize - the increments in workspace in which the robot tests for the next configuration.\n\n\
- minsteps - The minimum number of steps that need to be taken in order for success to declared. If robot doesn't reach this number of steps, it fails.\n\n\
- maxsteps - The maximum number of steps the robot should take.\n\n\
- direction - The workspace direction to move end effector in.\n\n\
Method wraps the WorkspaceTrajectoryTracker planner. For more details on parameters, check out its documentation.");
        RegisterCommand("MoveManipulator",boost::bind(&BaseManipulation::MoveManipulator,this,_1,_2),
                        "Moves arm joints of active manipulator to a given set of joint values");
        RegisterCommand("MoveActiveJoints",boost::bind(&BaseManipulation::MoveActiveJoints,this,_1,_2),
                        "Moves the current active joints to a specified goal destination\n");
        RegisterCommand("MoveToHandPosition",boost::bind(&BaseManipulation::MoveToHandPosition,this,_1,_2),
                        "Move the manipulator's end effector to some 6D pose.");
        RegisterCommand("MoveUnsyncJoints",boost::bind(&BaseManipulation::MoveUnsyncJoints,this,_1,_2),
                        "Moves the active joints to a position where the inactive (hand) joints can\n"
                        "fully move to their goal. This is necessary because synchronization with arm\n"
                        "and hand isn't guaranteed.\n"
                        "Options: handjoints savetraj planner");
        RegisterCommand("JitterActive",boost::bind(&BaseManipulation::JitterActive,this,_1,_2),
                        "Jitters the active DOF for a collision-free position.");
        RegisterCommand("FindIKWithFilters",boost::bind(&BaseManipulation::FindIKWithFilters,this,_1,_2),
                        "Samples IK solutions using custom filters that constrain the end effector in the world. Parameters:\n\n\
- cone - Constraint the direction of a local axis with respect to a cone in the world. Takes in: worldaxis(3), localaxis(3), anglelimit. \n\
- solveall - When specified, will return all possible solutions.\n\
- ikparam - The serialized ik parameterization to use for FindIKSolution(s).\n\
- filteroptions\n\
");
        _fMaxVelMult=1;
    }

    virtual ~BaseManipulation() {}

    virtual void Destroy()
    {
        robot.reset();
        ProblemInstance::Destroy();
    }

    virtual void Reset()
    {
        ProblemInstance::Reset();
    }

    virtual int main(const std::string& args)
    {
        string strRobotName;
        stringstream ss(args);
        ss >> strRobotName;
        robot = GetEnv()->GetRobot(strRobotName);

        _fMaxVelMult=1;
        string cmd;
        while(!ss.eof()) {
            ss >> cmd;
            if( !ss )
                break;
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
            if( cmd == "planner" )
                ss >> _strRRTPlannerName;
            else if( cmd == "maxvelmult" )
                ss >> _fMaxVelMult;

            if( ss.fail() || !ss )
                break;
        }

        PlannerBasePtr planner;
        if( _strRRTPlannerName.size() > 0 )
            planner = RaveCreatePlanner(GetEnv(),_strRRTPlannerName);
        if( !planner ) {
            _strRRTPlannerName = "BiRRT";
            planner = RaveCreatePlanner(GetEnv(),_strRRTPlannerName);
            if( !planner )
                _strRRTPlannerName = "";
        }

        RAVELOG_DEBUG(str(boost::format("BaseManipulation: using %s planner\n")%_strRRTPlannerName));
        return 0;
    }

    virtual bool SimulationStep(dReal fElapsedTime)
    {
        return false;
    }

    virtual bool SendCommand(std::ostream& sout, std::istream& sinput)
    {
        EnvironmentMutex::scoped_lock lock(GetEnv()->GetMutex());
        return ProblemInstance::SendCommand(sout,sinput);
    }
protected:

    inline boost::shared_ptr<BaseManipulation> shared_problem() { return boost::static_pointer_cast<BaseManipulation>(shared_from_this()); }
    inline boost::shared_ptr<BaseManipulation const> shared_problem_const() const { return boost::static_pointer_cast<BaseManipulation const>(shared_from_this()); }

    bool SetActiveManip(ostream& sout, istream& sinput)
    {
        string manipname;
        int index = -1;

        if(!sinput.eof()) {
            sinput >> manipname;
            if( !sinput ) {
                return false;
            }
            // find the manipulator with the right name
            index = 0;
            FOREACHC(itmanip, robot->GetManipulators()) {
                if( manipname == (*itmanip)->GetName() ) {
                    break;
                }
                ++index;
            }

            if( index >= (int)robot->GetManipulators().size() ) {
                index = atoi(manipname.c_str());
            }
        }

        if( index >= 0 && index < (int)robot->GetManipulators().size() ) {
            robot->SetActiveManipulator(index);
            return true;
        }

        return false;
    }

    bool Traj(ostream& sout, istream& sinput)
    {
        string filename; sinput >> filename;
        if( !sinput ) {
            return false;
        }
        TrajectoryBasePtr ptraj = RaveCreateTrajectory(GetEnv(),robot->GetDOF());
        char sep = ' ';
        if( filename == "sep" ) {
            sinput >> sep;
            filename = getfilename_withseparator(sinput,sep);
        }

        if( filename == "stream" ) {
            // the trajectory is embedded in the stream
            RAVELOG_VERBOSE("BaseManipulation: reading trajectory from stream\n");
            if( !ptraj->Read(sinput, robot) ) {
                RAVELOG_ERROR("BaseManipulation: failed to get trajectory\n");
                return false;
            }
        }
        else {
            RAVELOG_VERBOSE(str(boost::format("BaseManipulation: reading trajectory: %s\n")%filename));
            ifstream f(filename.c_str());
            if( !ptraj->Read(f, robot) ) {
                RAVELOG_ERROR(str(boost::format("BaseManipulation: failed to read trajectory %s\n")%filename));
                return false;
            }
        }
        
        bool bResetTrans = false; sinput >> bResetTrans;
        dReal fmaxvelmult = 1; sinput >> fmaxvelmult;
    
        if( bResetTrans ) {
            RAVELOG_VERBOSE("resetting transformations of trajectory\n");
            Transform tcur = robot->GetTransform();
            // set the transformation of every point to the current robot transformation
            FOREACH(itpoint, ptraj->GetPoints()) {
                itpoint->trans = tcur;
            }
        }

        if( ptraj->GetTotalDuration() == 0 ) {
            RAVELOG_VERBOSE(str(boost::format("retiming trajectory: %f\n")%fmaxvelmult));
            ptraj->CalcTrajTiming(robot,TrajectoryBase::CUBIC,true,false,fmaxvelmult);
        }
        RAVELOG_VERBOSE(str(boost::format("executing traj with %d points\n")%ptraj->GetPoints().size()));
        robot->SetMotion(ptraj);
        sout << "1";
        return true;
    }

    bool MoveHandStraight(ostream& sout, istream& sinput)
    {
        Vector direction = Vector(0,1,0);
        string strtrajfilename;
        bool bExecute = true;
        int minsteps = 0;
        int maxsteps = 10000;
        bool starteematrix = false;

        RobotBase::ManipulatorConstPtr pmanip = robot->GetActiveManipulator();
        Transform Tee;

        boost::shared_ptr<WorkspaceTrajectoryParameters> params(new WorkspaceTrajectoryParameters(GetEnv()));
        boost::shared_ptr<ostream> pOutputTrajStream;
        params->ignorefirstcollision = 0.04; // 0.04m?
        string cmd;
        params->_fStepLength = 0.01;
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
            if( cmd == "minsteps" ) {
                sinput >> minsteps;
            }
            else if( cmd == "outputtraj") {
                pOutputTrajStream = boost::shared_ptr<ostream>(&sout,null_deleter());
            }
            else if( cmd == "maxsteps") {
                sinput >> maxsteps;
            }
            else if( cmd == "stepsize") {
                sinput >> params->_fStepLength;
            }
            else if( cmd == "execute" ) {
                sinput >> bExecute;
            }
            else if( cmd == "writetraj") {
                sinput >> strtrajfilename;
            }
            else if( cmd == "direction") {
                sinput >> direction.x >> direction.y >> direction.z;
                direction.normalize3();
            }
            else if( cmd == "ignorefirstcollision") {
                sinput >> params->ignorefirstcollision;
            }
            else if( cmd == "greedysearch" ) {
                sinput >> params->greedysearch;
            }
            else if( cmd == "maxdeviationangle" ) {
                sinput >> params->maxdeviationangle;
            }
            else if( cmd == "jacobian" ) {
                RAVELOG_WARN("MoveHandStraight jacobian parameter not supported anymore\n");
            }
            else if( cmd == "starteematrix" ) {
                TransformMatrix tm;
                starteematrix = true;
                sinput >> tm;
                Tee = tm;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        params->minimumcompletetime = params->_fStepLength * minsteps;
        RAVELOG_DEBUG("Starting MoveHandStraight dir=(%f,%f,%f)...\n",(float)direction.x, (float)direction.y, (float)direction.z);
        robot->RegrabAll();

        RobotBase::RobotStateSaver saver(robot);

        robot->SetActiveDOFs(pmanip->GetArmIndices());
        params->SetRobotActiveJoints(robot);

        if( !starteematrix ) {
            CM::JitterActiveDOF(robot,100); // try to jitter out, don't worry if it fails
            robot->GetActiveDOFValues(params->vinitialconfig);
            Tee = pmanip->GetEndEffectorTransform();
        }
        else {
            params->vinitialconfig.resize(0); // set by SetRobotActiveJoints
        }

        // compute a workspace trajectory (important to do this after jittering!)
        {
            Vector voldtrans = robot->GetAffineTranslationMaxVels();
            dReal foldrot = robot->GetAffineRotationQuatMaxVels();
            robot->SetAffineTranslationMaxVels(Vector(1,1,1));
            robot->SetAffineRotationQuatMaxVels(1.0);
            params->workspacetraj = RaveCreateTrajectory(GetEnv(),"");
            params->workspacetraj->Reset(0);
            params->workspacetraj->AddPoint(TrajectoryBase::TPOINT(vector<dReal>(),Tee,0));
            Tee.trans += direction*maxsteps*params->_fStepLength;
            params->workspacetraj->AddPoint(TrajectoryBase::TPOINT(vector<dReal>(),Tee,0));
            params->workspacetraj->CalcTrajTiming(RobotBasePtr(),TrajectoryBase::LINEAR,true,false);
            robot->SetAffineTranslationMaxVels(voldtrans);
            robot->SetAffineRotationQuatMaxVels(foldrot);
        }

        boost::shared_ptr<PlannerBase> planner = RaveCreatePlanner(GetEnv(),"workspacetrajectorytracker");
        if( !planner ) {
            RAVELOG_WARN("failed to create planner\n");
            return false;
        }
    
        if( !planner->InitPlan(robot, params) ) {
            RAVELOG_ERROR("InitPlan failed\n");
            return false;
        }

        boost::shared_ptr<Trajectory> poutputtraj(RaveCreateTrajectory(GetEnv(),""));
        if( !planner->PlanPath(poutputtraj) ) {
            return false;
        }
        CM::SetActiveTrajectory(robot, poutputtraj, bExecute, strtrajfilename, pOutputTrajStream,_fMaxVelMult);
        return true;
    }

    bool MoveManipulator(ostream& sout, istream& sinput)
    {
        RAVELOG_DEBUG("Starting MoveManipulator...\n");
        RobotBase::ManipulatorPtr pmanip = robot->GetActiveManipulator();

        string strtrajfilename;
        bool bExecute = true;
        boost::shared_ptr<ostream> pOutputTrajStream;
        std::vector<dReal> goals;
        PlannerBase::PlannerParametersPtr params(new PlannerBase::PlannerParameters());
        params->_nMaxIterations = 4000; // max iterations before failure

        string cmd;
        int nMaxTries = 3; // max tries for the planner
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if( cmd == "armvals" || cmd == "goal" ) {
                goals.resize(pmanip->GetArmIndices().size());
                FOREACH(it, goals) {
                    sinput >> *it;
                }
            }
            else if( cmd == "outputtraj" ) {
                pOutputTrajStream = boost::shared_ptr<ostream>(&sout,null_deleter());
            }
            else if( cmd == "maxiter" ) {
                sinput >> params->_nMaxIterations;
            }
            else if( cmd == "execute" ) {
                sinput >> bExecute;
            }
            else if( cmd == "writetraj" ) {
                sinput >> strtrajfilename;
            }
            else if( cmd == "maxtries" ) {
                sinput >> nMaxTries;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }
    
        if( goals.size() != pmanip->GetArmIndices().size() ) {
            return false;
        }

        RobotBase::RobotStateSaver saver(robot);

        robot->SetActiveDOFs(pmanip->GetArmIndices());
        params->SetRobotActiveJoints(robot);
        CM::JitterActiveDOF(robot);
    
        boost::shared_ptr<Trajectory> ptraj(RaveCreateTrajectory(GetEnv(),robot->GetActiveDOF()));

        std::vector<dReal> values;
        robot->GetActiveDOFValues(values);

        // make sure the initial and goal configs are not in collision
        robot->SetActiveDOFValues(goals, true);
        if( CM::JitterActiveDOF(robot) == 0 ) {
            RAVELOG_WARN("jitter failed\n");
            return false;
        }
        robot->GetActiveDOFValues(params->vgoalconfig);
        robot->SetActiveDOFValues(values);
    
        // jitter again for initial collision
        if( CM::JitterActiveDOF(robot) == 0 ) {
            RAVELOG_WARN("jitter failed\n");
            return false;
        }
        robot->GetActiveDOFValues(params->vinitialconfig);

        boost::shared_ptr<PlannerBase> rrtplanner = RaveCreatePlanner(GetEnv(),_strRRTPlannerName);
        if( !rrtplanner ) {
            RAVELOG_WARN("failed to create planner\n");
            return false;
        }
    
        bool bSuccess = false;
        RAVELOG_INFO("starting planning\n");
    
        for(int iter = 0; iter < nMaxTries; ++iter) {
            if( !rrtplanner->InitPlan(robot, params) ) {
                RAVELOG_ERROR("InitPlan failed\n");
                break;
            }
        
            if( rrtplanner->PlanPath(ptraj) ) {
                bSuccess = true;
                RAVELOG_INFO("finished planning\n");
                break;
            }
            else RAVELOG_WARN("PlanPath failed\n");
        }

        if( !bSuccess ) {
            return false;
        }
        CM::SetActiveTrajectory(robot, ptraj, bExecute, strtrajfilename, pOutputTrajStream,_fMaxVelMult);
        sout << "1";
        return true;
    }

    bool MoveActiveJoints(ostream& sout, istream& sinput)
    {
        string strtrajfilename;
        bool bExecute = true;
        int nMaxTries = 1; // max tries for the planner
        boost::shared_ptr<ostream> pOutputTrajStream;
    
        PlannerBase::PlannerParametersPtr params(new PlannerBase::PlannerParameters());
        params->_nMaxIterations = 4000; // max iterations before failure

        string cmd;
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
            if( cmd == "goal" ) {
                params->vgoalconfig.resize(robot->GetActiveDOF());
                FOREACH(it, params->vgoalconfig) {
                    sinput >> *it;
                }
            }
            else if( cmd == "outputtraj" ) {
                pOutputTrajStream = boost::shared_ptr<ostream>(&sout,null_deleter());
            }
            else if( cmd == "maxiter" ) {
                sinput >> params->_nMaxIterations;
            }
            else if( cmd == "execute" ) {
                sinput >> bExecute;
            }
            else if( cmd == "writetraj" ) {
                sinput >> strtrajfilename;
            }
            else if( cmd == "steplength" ) {
                sinput >> params->_fStepLength;
            }
            else if( cmd == "maxtries" ) {
                sinput >> nMaxTries;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        if( (int)params->vgoalconfig.size() != robot->GetActiveDOF() )
            return false;
    
        RobotBase::RobotStateSaver saver(robot);

        if( CM::JitterActiveDOF(robot) == 0 ) {
            RAVELOG_WARN("failed\n");
            return false;
        }

        // restore
        params->SetRobotActiveJoints(robot);
        robot->GetActiveDOFValues(params->vinitialconfig);
        robot->SetActiveDOFValues(params->vgoalconfig);
    
        // jitter again for goal
        if( CM::JitterActiveDOF(robot) == 0 ) {
            RAVELOG_WARN("failed\n");
            return false;
        }

        boost::shared_ptr<PlannerBase> rrtplanner = RaveCreatePlanner(GetEnv(),_strRRTPlannerName);

        if( !rrtplanner ) {
            RAVELOG_ERROR("failed to create BiRRTs\n");
            return false;
        }
    
        boost::shared_ptr<Trajectory> ptraj(RaveCreateTrajectory(GetEnv(),robot->GetActiveDOF()));
    
        RAVELOG_DEBUG("starting planning\n");
        bool bSuccess = false;
        for(int itry = 0; itry < nMaxTries; ++itry) {
            if( !rrtplanner->InitPlan(robot, params) ) {
                RAVELOG_ERROR("InitPlan failed\n");
                return false;
            }
            
            if( !rrtplanner->PlanPath(ptraj) ) {
                RAVELOG_WARN("PlanPath failed\n");
            }
            else {
                bSuccess = true;
                RAVELOG_DEBUG("finished planning\n");
                break;
            }
        }

        if( !bSuccess )
            return false;
        CM::SetActiveTrajectory(robot, ptraj, bExecute, strtrajfilename, pOutputTrajStream,_fMaxVelMult);
        return true;
    }

    bool MoveToHandPosition(ostream& sout, istream& sinput)
    {
        RAVELOG_DEBUG("Starting MoveToHandPosition...\n");
        RobotBase::ManipulatorConstPtr pmanip = robot->GetActiveManipulator();
        
        list<IkParameterization> listgoals;
    
        string strtrajfilename;
        bool bExecute = true;
        boost::shared_ptr<ostream> pOutputTrajStream;

        Vector vconstraintaxis, vconstraintpos;
        int affinedofs = 0;
        int nSeedIkSolutions = 0; // no extra solutions
        int nMaxTries = 3; // max tries for the planner

        PlannerBase::PlannerParametersPtr params(new PlannerBase::PlannerParameters());
        params->_nMaxIterations = 4000;

        // constraint stuff
        boost::array<double,6> vconstraintfreedoms = {{0,0,0,0,0,0}};
        Transform tConstraintTargetWorldFrame;
        double constrainterrorthresh=0;

        string cmd;
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
            if( cmd == "translation" ) {
                Vector trans;
                sinput >> trans.x >> trans.y >> trans.z;
                listgoals.push_back(IkParameterization());
                listgoals.back().SetTranslation3D(trans);
            }
            else if( cmd == "rotation" ) {
                Vector q;
                sinput >> q.x >> q.y >> q.z >> q.w;
                listgoals.push_back(IkParameterization());
                listgoals.back().SetRotation3D(q);
            }
            else if( cmd == "outputtraj" ) {
                pOutputTrajStream = boost::shared_ptr<ostream>(&sout,null_deleter());
            }
            else if( cmd == "matrix" ) {
                TransformMatrix m;
                sinput >> m;
                listgoals.push_back(IkParameterization(Transform(m)));
            }
            else if( cmd == "matrices" ) {
                TransformMatrix m;
                int num = 0;
                sinput >> num;
                while(num-->0) {
                    sinput >> m;
                    listgoals.push_back(IkParameterization(Transform(m)));
                }
            }
            else if( cmd == "pose" ) {
                Transform t;
                sinput >> t;
                listgoals.push_back(IkParameterization(t));
            }
            else if( cmd == "poses" ) {
                int num = 0;
                sinput >> num;
                while(num-->0) {
                    Transform t;
                    sinput >> t;
                    listgoals.push_back(IkParameterization(t));
                }
            }
            else if( cmd == "affinedofs" ) {
                sinput >> affinedofs;
            }
            else if( cmd == "maxiter" ) {
                sinput >> params->_nMaxIterations;
            }
            else if( cmd == "maxtries" ) {
                sinput >> nMaxTries;
            }
            else if( cmd == "execute" ) {
                sinput >> bExecute;
            }
            else if( cmd == "writetraj" ) {
                sinput >> strtrajfilename;
            }
            else if( cmd == "seedik" ) {
                sinput >> nSeedIkSolutions;
            }
            else if( cmd == "constraintfreedoms" ) {
                FOREACH(it,vconstraintfreedoms) {
                    sinput >> *it;
                }
            }
            else if( cmd == "constraintmatrix" ) {
                TransformMatrix m; sinput >> m; tConstraintTargetWorldFrame = m;
            }
            else if( cmd == "constraintpose" ) {
                sinput >> tConstraintTargetWorldFrame;
            }
            else if( cmd == "constrainterrorthresh" ) {
                sinput >> constrainterrorthresh;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        robot->RegrabAll();
        RobotBase::RobotStateSaver saver(robot);

        std::vector<dReal> viksolution, armgoals;
        if( nSeedIkSolutions < 0 ) {
            vector<vector<dReal> > solutions;
            FOREACH(ittrans, listgoals) {
                pmanip->FindIKSolutions(*ittrans, solutions, true);            
                armgoals.reserve(armgoals.size()+solutions.size()*pmanip->GetArmIndices().size());
                FOREACH(itsol, solutions) {
                    armgoals.insert(armgoals.end(), itsol->begin(), itsol->end());
                }
            }
        }
        else if( nSeedIkSolutions > 0 ) {
            FOREACH(ittrans, listgoals) {
                int nsampled = CM::SampleIkSolutions(robot, *ittrans, nSeedIkSolutions, armgoals);
                if( nsampled != nSeedIkSolutions ) {
                    RAVELOG_WARN("only found %d/%d ik solutions\n", nsampled, nSeedIkSolutions);
                }
            }
        }
        else {
            FOREACH(ittrans, listgoals) {
                if( pmanip->FindIKSolution(*ittrans, viksolution, true) ) {
                    stringstream s;
                    s << "ik sol: ";
                    FOREACH(it, viksolution) {
                        s << *it << " ";
                    }
                    s << endl;
                    RAVELOG_DEBUG(s.str());
                    armgoals.insert(armgoals.end(), viksolution.begin(), viksolution.end());
                }
            }
        }

        if( armgoals.size() == 0 ) {
            RAVELOG_WARN("No IK Solution found\n");
            return false;
        }

        RAVELOG_INFO(str(boost::format("MoveToHandPosition found %d solutions\n")%(armgoals.size()/pmanip->GetArmIndices().size())));
    
        robot->SetActiveDOFs(pmanip->GetArmIndices(), affinedofs);
        params->SetRobotActiveJoints(robot);
        robot->GetActiveDOFValues(params->vinitialconfig);

        if( constrainterrorthresh > 0 ) {
            RAVELOG_DEBUG("setting jacobian constraint function in planner parameters\n");
            boost::shared_ptr<CM::GripperJacobianConstrains<double> > pconstraints(new CM::GripperJacobianConstrains<double>(robot->GetActiveManipulator(),tConstraintTargetWorldFrame,vconstraintfreedoms,constrainterrorthresh));
            pconstraints->_distmetricfn = params->_distmetricfn;
            params->_constraintfn = boost::bind(&CM::GripperJacobianConstrains<double>::RetractionConstraint,pconstraints,_1,_2,_3);
        }

        robot->SetActiveDOFs(pmanip->GetArmIndices(), 0);

        vector<dReal> vgoals;
        params->vgoalconfig.reserve(armgoals.size());
        for(int i = 0; i < (int)armgoals.size(); i += pmanip->GetArmIndices().size()) {
            vector<dReal> v(armgoals.begin()+i,armgoals.begin()+i+pmanip->GetArmIndices().size());
            robot->SetActiveDOFValues(v);
            robot->SetActiveDOFs(pmanip->GetArmIndices(), affinedofs);

            if( CM::JitterActiveDOF(robot,5000,0.03,params->_constraintfn) ) {
                robot->GetActiveDOFValues(vgoals);
                params->vgoalconfig.insert(params->vgoalconfig.end(), vgoals.begin(), vgoals.end());
            }
            else {
                RAVELOG_DEBUG("constraint function failed for goal %d\n",i);
            }
        }

        if( params->vgoalconfig.size() == 0 ) {
            RAVELOG_WARN("jitter failed for goal\n");
            return false;
        }

        // restore
        robot->SetActiveDOFValues(params->vinitialconfig);

        boost::shared_ptr<Trajectory> ptraj(RaveCreateTrajectory(GetEnv(),robot->GetActiveDOF()));

        Trajectory::TPOINT pt;
        pt.q = params->vinitialconfig;
        ptraj->AddPoint(pt);
    
        // jitter again for initial collision
        if( CM::JitterActiveDOF(robot,5000,0.03,params->_constraintfn) == 0 ) {
            RAVELOG_WARN("jitter failed for initial\n");
            return false;
        }
        robot->GetActiveDOFValues(params->vinitialconfig);

        boost::shared_ptr<PlannerBase> rrtplanner = RaveCreatePlanner(GetEnv(),_strRRTPlannerName);
        if( !rrtplanner ) {
            RAVELOG_ERROR("failed to create BiRRTs\n");
            return false;
        }
    
        bool bSuccess = false;
        RAVELOG_INFO("starting planning\n");
        
        for(int iter = 0; iter < nMaxTries; ++iter) {
            if( !rrtplanner->InitPlan(robot, params) ) {
                RAVELOG_ERROR("InitPlan failed\n");
                return false;
            }
        
            if( rrtplanner->PlanPath(ptraj) ) {
                bSuccess = true;
                RAVELOG_INFO("finished planning\n");
                break;
            }
            else
                RAVELOG_WARN("PlanPath failed\n");
        }

        rrtplanner.reset(); // have to destroy before environment
    
        if( !bSuccess ) {
            return false;
        }
        CM::SetActiveTrajectory(robot, ptraj, bExecute, strtrajfilename, pOutputTrajStream,_fMaxVelMult);
        sout << "1";
        return true;
    }

    bool MoveUnsyncJoints(ostream& sout, istream& sinput)
    {
        string strplanner = "BasicRRT";
        string strsavetraj;
        string cmd;
        vector<int> vhandjoints;
        vector<dReal> vhandgoal;
        bool bExecute = true;
        boost::shared_ptr<ostream> pOutputTrajStream;
        int nMaxTries=1;
        int maxdivision=10;
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if( cmd == "writetraj" ) {
                sinput >> strsavetraj;
            }
            else if( cmd == "outputtraj" ) {
                pOutputTrajStream = boost::shared_ptr<ostream>(&sout,null_deleter());
            }
            else if( cmd == "handjoints" ) {
                int dof = 0;
                sinput >> dof;
                if( !sinput || dof == 0 ) {
                    return false;
                }
                vhandjoints.resize(dof);
                vhandgoal.resize(dof);
                FOREACH(it, vhandgoal) {
                    sinput >> *it;
                }
                FOREACH(it, vhandjoints) {
                    sinput >> *it;
                }
            }
            else if( cmd == "planner" ) {
                sinput >> strplanner;
            }
            else if( cmd == "execute" ) {
                sinput >> bExecute;
            }
            else if( cmd == "maxtries" ) {
                sinput >> nMaxTries;
            }
            else if( cmd == "maxdivision" ) {
                sinput >> maxdivision;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        uint32_t starttime = timeGetTime();

        if( CM::JitterActiveDOF(robot) == 0 ) {
            RAVELOG_WARN("failed to jitter robot out of collision\n");
        }

        boost::shared_ptr<Trajectory> ptraj(RaveCreateTrajectory(GetEnv(),robot->GetActiveDOF()));
    
        bool bSuccess = false;
        for(int itry = 0; itry < nMaxTries; ++itry) {
            if( CM::MoveUnsync::_MoveUnsyncJoints(GetEnv(), robot, ptraj, vhandjoints, vhandgoal, strplanner,maxdivision) ) {
                bSuccess = true;
                break;
            }
        }
        if( !bSuccess ) {
            return false;
        }

        BOOST_ASSERT(ptraj->GetPoints().size() > 0);

        bool bExecuted = CM::SetActiveTrajectory(robot, ptraj, bExecute, strsavetraj, pOutputTrajStream,_fMaxVelMult);
        sout << (int)bExecuted << " ";

        sout << (timeGetTime()-starttime)/1000.0f << " ";
        FOREACH(it, ptraj->GetPoints().back().q) {
            sout << *it << " ";
        }

        return true;
    }

    bool JitterActive(ostream& sout, istream& sinput)
    {
        RAVELOG_DEBUG("Starting ReleaseFingers...\n");
        bool bExecute = true, bOutputFinal=false;
        boost::shared_ptr<ostream> pOutputTrajStream;
        string cmd;
        int nMaxIterations=5000;
        dReal fJitter=0.03f;
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if( cmd == "execute" ) {
                sinput >> bExecute;
            }
            else if( cmd == "maxiter" ) {
                sinput >> nMaxIterations;
            }
            else if( cmd == "jitter" ) {
                sinput >> fJitter;
            }
            else if( cmd == "outputtraj" ) {
                pOutputTrajStream = boost::shared_ptr<ostream>(&sout,null_deleter());
            }
            else if( cmd == "outputfinal" ) {
                bOutputFinal = true;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        RobotBase::RobotStateSaver saver(robot);
        boost::shared_ptr<Trajectory> ptraj(RaveCreateTrajectory(GetEnv(),robot->GetActiveDOF()));

        // have to add the first point
        Trajectory::TPOINT ptfirst;
        robot->GetActiveDOFValues(ptfirst.q);
        ptraj->AddPoint(ptfirst);
        switch( CM::JitterActiveDOF(robot,nMaxIterations,fJitter) ) {
        case 0:
            RAVELOG_WARN("could not jitter out of collision\n");
            return false;
        case 1:
            robot->GetActiveDOFValues(ptfirst.q);
            ptraj->AddPoint(ptfirst);
        default:
            break;
        }

        if( bOutputFinal ) {
            FOREACH(itq,ptfirst.q) {
                sout << *itq << " ";
            }
        }

        CM::SetActiveTrajectory(robot, ptraj, bExecute, "", pOutputTrajStream,_fMaxVelMult);
        return true;
    }

    class IkResetFilter
    {
    public:
        IkResetFilter(IkSolverBasePtr iksolver) : _iksolver(iksolver) {}
        virtual ~IkResetFilter() { _iksolver->SetCustomFilter(IkSolverBase::IkFilterCallbackFn()); }
        IkSolverBasePtr _iksolver;
    };

    bool FindIKWithFilters(ostream& sout, istream& sinput)
    {
        bool bSolveAll = false;
        IkSolverBase::IkFilterCallbackFn filterfn;
        IkParameterization ikparam;
        int filteroptions = IKFO_CheckEnvCollisions;
        string cmd;
        RobotBase::ManipulatorPtr pmanip = robot->GetActiveManipulator();
        if( !pmanip->GetIkSolver() ) {
            throw openrave_exception(str(boost::format("FindIKWithFilters: manipulator %s has no ik solver set")%robot->GetActiveManipulator()->GetName()));
        }

        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

            if( cmd == "cone" ) {
                Vector vlocalaxis, vworldaxis;
                dReal anglelimit;
                sinput >> vlocalaxis.x >> vlocalaxis.y >> vlocalaxis.z >> vworldaxis.x >> vworldaxis.y >> vworldaxis.z >> anglelimit;
                filterfn = boost::bind(&BaseManipulation::_FilterWorldAxisIK,shared_problem(),_1,_2,_3, vlocalaxis, vworldaxis, RaveCos(anglelimit));
            }
            else if( cmd == "solveall" ) {
                bSolveAll = true;
            }
            else if( cmd == "ikparam" ) {
                sinput >> ikparam;
            }
            else if( cmd == "filteroptions" ) {
                sinput >> filteroptions;
            }
            else {
                RAVELOG_WARN(str(boost::format("unrecognized command: %s\n")%cmd));
                break;
            }
            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        if( !filterfn ) {
            throw openrave_exception("FindIKWithFilters: no filter function set");
        }
        IkResetFilter resetfilter(robot->GetActiveManipulator()->GetIkSolver());
        pmanip->GetIkSolver()->SetCustomFilter(filterfn);
        vector< vector<dReal> > vsolutions;
        if( bSolveAll ) {
            if( !pmanip->FindIKSolutions(ikparam,vsolutions,filteroptions)) {
                return false;
            }
        }
        else {
            vsolutions.resize(1);
            if( !pmanip->FindIKSolution(ikparam,vsolutions[0],filteroptions)) {
                return false;
            }
        }
        sout << vsolutions.size() << " ";
        FOREACH(itsol,vsolutions) {
            FOREACH(it, *itsol) {
                sout << *it << " ";
            }
        }
        return true;
    }

    bool GrabBody(ostream& sout, istream& sinput)
    {
        RAVELOG_WARN("BaseManipulation GrabBody command is deprecated. Use Robot::Grab (11/03/07)\n");

        KinBodyPtr ptarget;

        string cmd;
        while(!sinput.eof()) {
            sinput >> cmd;
            if( !sinput ) {
                break;
            }
            std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
            if( cmd == "name" ) {
                string name;
                sinput >> name;
                ptarget = GetEnv()->GetKinBody(name);
            }
            else {
                break;
            }

            if( !sinput ) {
                RAVELOG_ERROR(str(boost::format("failed processing command %s\n")%cmd));
                return false;
            }
        }

        if(!ptarget) {
            RAVELOG_ERROR("ERROR Manipulation::GrabBody - Invalid body name.\n");
            return false;
        }

        RAVELOG_DEBUG(str(boost::format("robot %s:%s grabbing body %s...\n")%robot->GetName()%robot->GetActiveManipulator()->GetEndEffector()->GetName()%ptarget->GetName()));
        robot->Grab(ptarget);
        return true;
    }

    bool ReleaseAll(ostream& sout, istream& sinput)
    {
        RAVELOG_WARN("BaseManipulation ReleaseAll command is deprecated. Use Robot::ReleaseAllGrabbed (11/03/07)\n");
        if( !!robot ) {
            RAVELOG_DEBUG("Releasing all bodies\n");
            robot->ReleaseAllGrabbed();
        }
        return true;
    }

 protected:
    IkFilterReturn _FilterWorldAxisIK(std::vector<dReal>& values, RobotBase::ManipulatorPtr pmanip, const IkParameterization& ikparam, const Vector& vlocalaxis, const Vector& vworldaxis, dReal coslimit)
    {
        if( RaveFabs(vworldaxis.dot3(pmanip->GetEndEffectorTransform().rotate(vlocalaxis))) < coslimit ) {
            return IKFR_Reject;
        }
        return IKFR_Success;
    }

    RobotBasePtr robot;
    string _strRRTPlannerName;
    dReal _fMaxVelMult;
};

ProblemInstancePtr CreateBaseManipulation(EnvironmentBasePtr penv) { return ProblemInstancePtr(new BaseManipulation(penv)); }