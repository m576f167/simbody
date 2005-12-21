/**@file
 * This file contains all the multibody mechanics code that involves a single body and
 * its inboard joint, that is, one node in the multibody tree.
 *
 * Most methods here expect to be called in a particular order during traversal of the
 * tree -- either base to tip or tip to base.
 */

#include "RigidBodyNode.h"
#include "cdsVec4.h"

#include "cdsMath.h"
#include "cdsVector.h"
#include "fixedVector.h"
#include "subVector.h"
#include "subMatrix.h"
#include "matrixTools.h"
#include "cdsAuto_ptr.h"

#include "cdsIomanip.h"

#ifdef USE_CDS_NAMESPACE 
using namespace CDS;
using MatrixTools::inverse;
using CDSMath::sq;
#endif /* USE_CDS_NAMESPACE */

// NOTE: the current mechanism of chosing between MatrixTools::transpose
// and that defined in phiMatrix.hh is ugly, but seems required is order
// for gcc-2.95 to compile this source.

typedef FixedMatrix<double,2,3> Mat23;
typedef FixedVector<double,5>   CDSVec5;


typedef SubVector<RVec>       RSubVec;
typedef SubVector<const RVec> ConstRSubVec;
typedef SubVector<CDSVec4>    RSubVec4;
typedef SubVector<CDSVec5>       RSubVec5;
typedef SubVector<CDSVec6>       RSubVec6;
typedef SubVector<const CDSVec6> ConstRSubVec6;

static Mat23 catRow23(const CDSVec3& v1, const CDSVec3& v2);
static CDSMat33 makeJointFrameFromZAxis(const CDSVec3& zVec);
static CDSMat33 makeIdentity33();
static const CDSMat33 ident33 = makeIdentity33(); // handy to have around
static const CDSMat33 zero33(0.);

//////////////////////////////////////////////
// Implementation of RigidBodyNode methods. //
//////////////////////////////////////////////

void RigidBodyNode::addChild(RigidBodyNode* child, const RBFrame& referenceFrame) {
    children.append( child );
    child->setParent(this);
    child->refOrigin_P = referenceFrame.getLoc_RF();    // ignore frame for now, it's always identity
    child->R_GB = R_GB;
    child->OB_G = OB_G + child->refOrigin_P;
    child->COM_G = child->OB_G + child->COMstation_G;
}

//
// Calc posCM, mass, Mk
//      phi, inertia
// Should be calc'd from base to tip.
void RigidBodyNode::calcJointIndependentKinematicsPos() {
    // Re-express parent-to-child shift vector (OB-OP) into the ground frame.
    const CDSVec3 OB_OP_G = getR_GP()*OB_P;

    // The Phi matrix conveniently performs parent-to-child shifting
    // on spatial quantities.
    phi = PhiMatrix(OB_OP_G);

    // Get spatial configuration of this body.
    R_GB = getR_GP() * R_PB;
    OB_G = getOP_G() + OB_OP_G;

    // Calculate spatial mass properties. That means we need to transform
    // the local mass moments into the Ground frame and reconstruct the
    // spatial inertia matrix Mk.

    inertia_OB_G = RBInertia(orthoTransform(getInertia_OB_B(), getR_GB()));
    COMstation_G = getR_GB()*getCOM_B();

    COM_G = OB_G + COMstation_G;

    // Calc Mk: the spatial inertia matrix about the body origin.
    // Note that this is symmetric; offDiag is *skew* symmetric so
    // that transpose(offDiag) = -offDiag.
    const CDSMat33 offDiag = getMass()*crossMat(COMstation_G);
    Mk = blockMat22( inertia_OB_G , offDiag ,
                    -offDiag      , getMass()*ident33 );
}

// Calculate velocity-related quantities: spatial velocity (sVel),
// gyroscopic force b, coriolis acceleration a. This must be
// called base to tip: depends on parent's sVel, V_PB_G.
void 
RigidBodyNode::calcJointIndependentKinematicsVel() {
    setSpatialVel(transpose(phi) * parent->getSpatialVel()
                  + V_PB_G);
    const CDSVec3& omega = getSpatialAngVel();
    const CDSVec3 gMoment = cross(omega, inertia_OB_G * omega);
    const CDSVec3 gForce  = getMass() * cross(omega, 
                                           cross(omega, COMstation_G));
    b = blockVec(gMoment, gForce);

    const CDSVec3& vel    = getSpatialLinVel();
    const CDSVec3& pOmega = parent->getSpatialAngVel();
    const CDSVec3& pVel   = parent->getSpatialLinVel();

    // calc a: coriolis acceleration
    a = blockMat22(crossMat(pOmega),   CDSMat33(0.0),
                      CDSMat33(0.0)   , crossMat(pOmega)) 
        * V_PB_G;
    a += blockVec(CDSVec3(0.0), cross(pOmega, vel-pVel));
}

double RigidBodyNode::calcKineticEnergy() const {
    double ret = dot(sVel , Mk*sVel);
    return 0.5*ret;
}


void RigidBodyNode::nodeDump(ostream& o) const {
    o << "NODE DUMP level=" << level << " type=" << type() << endl;
    nodeSpecDump(o);
    o << "END OF NODE type=" << type() << endl;
}

ostream& operator<<(ostream& s, const RigidBodyNode& node) {
    node.nodeDump(s);
    return s;
}


////////////////////////////////////////////////
// Define classes derived from RigidBodyNode. //
////////////////////////////////////////////////

/**
 * This is the distinguished body representing the immobile ground frame. Other bodies may
 * be fixed to this one, but only this is the actual Ground.
 */
class RBGroundBody : public RigidBodyNode {
public:
    RBGroundBody() // TODO: should set mass properties to infinity
      : RigidBodyNode(RBMassProperties(),CDSVec3(0.),ident33,CDSVec3(0.)) {}
    ~RBGroundBody() {}

    /*virtual*/const char* type() const { return "ground"; }

    /*virtual*/void calcP() {} 
    /*virtual*/void calcZ(const CDSVec6&) {} 
    /*virtual*/void calcY() {}
    /*virtual*/void calcInternalForce(const CDSVec6&) {}
    /*virtual*/void calcAccel() {}

    /*virtual*/void setPos(const RVec&) {}
    /*virtual*/void setVel(const RVec&) {}
    /*virtual*/void setVelFromSVel(const CDSVec6&) {}
    /*virtual*/void enforceConstraints(RVec& pos, RVec& vel) {}

    /*virtual*/void getPos(RVec&)   const {}
    /*virtual*/void getVel(RVec&)   const {}
    /*virtual*/void getAccel(RVec&) const {}

    /*virtual*/void getInternalForce(RVec&) const {}
    // virtual RMat getH()

    void print(int) {}
};

template<int dof>
class RigidBodyNodeSpec : public RigidBodyNode {
public:
    RigidBodyNodeSpec(const RBMassProperties& mProps_B,
                      const RBFrame& jointFrame,
                      int& cnt)
      : RigidBodyNode(mProps_B,CDSVec3(0.),jointFrame.getRot_RF(),jointFrame.getLoc_RF()),
        theta(0.), dTheta(0.), ddTheta(0.), forceInternal(0.)
    {
        stateOffset = cnt;
        cnt += dof;
    }

    virtual ~RigidBodyNodeSpec() {}

    /// Calculate joint-specific kinematic quantities dependent only
    /// on positions. This routine may assume that *all* position 
    /// kinematics (not just joint-specific) has been done for the parent,
    /// and that the position state variables (theta) are available. The
    /// quanitites that must be computed are:
    ///   R_PB  the orientation of the B frame in its parent's frame
    ///   OB_P  the location of B's origin in its parent (meas. from OP)
    ///   H     the joint transition matrix
    virtual void calcJointKinematicsPos()=0;

    /// Calculate joint-specific kinematic quantities dependent on
    /// on velocities. This routine may assume that *all* position 
    /// kinematics (not just joint-specific) has been done for this node,
    /// that all velocity kinematics has been done for the parent, and
    /// that the velocity state variables (dTheta) are available. The
    /// quanitites that must be computed are:
    ///   V_PB_G  relative velocity of B in P, expr. in G
    virtual void calcJointKinematicsVel()=0;

    /// Set a new configuration and calculate the consequent kinematics.
    void setPos(const RVec& posv) {
        forceInternal.set(0.);  // forget these
        setJointPos(posv);
        calcJointKinematicsPos();
        calcJointIndependentKinematicsPos();
    }

    /// Set new velocities for the current configuration, and calculate
    /// all the velocity-dependent terms.
    void setVel(const RVec& velv) {
        // anything to initialize?
        setJointVel(velv);
        calcJointKinematicsVel();
        calcJointIndependentKinematicsVel();
    }

    // These unfortunately need to be overridden for joints using quaternions.
    virtual void setJointPos(const RVec& posv) {
        theta  = ConstRSubVec(posv,stateOffset,dof).vector();
    }
    virtual void setJointVel(const RVec& velv) {
        dTheta = ConstRSubVec(velv,stateOffset,dof).vector();
    }
    virtual void calcJointAccel() { }

    virtual void getPos(RVec& p) const {
        RSubVec(p,stateOffset,dof) = theta.vector();
    }
    virtual void getVel(RVec& v) const {
        RSubVec(v,stateOffset,dof) = dTheta.vector();
    }   
    virtual void getAccel(RVec& a) const {
        RSubVec(a,stateOffset,dof) = ddTheta.vector();
    }
    virtual void getInternalForce(RVec& t) const {
        RSubVec(t,stateOffset,dof) = forceInternal.vector();
    }

    int          getDOF() const { return dof; }
    virtual int  getDim() const { return dof; } // dim can be larger than dof

    virtual void   print(int) const;


    virtual void setVelFromSVel(const CDSVec6&);
    virtual void enforceConstraints(RVec& pos, RVec& vel) {}

    virtual RMat getH() const { return RMat(H); }

    void calcP();
    void calcZ(const CDSVec6& spatialForce);
    void calcY();
    void calcAccel();
    void calcInternalForce(const CDSVec6& spatialForce);

    void nodeSpecDump(ostream& o) const {
        o << "stateOffset=" << stateOffset << " mass=" << getMass() 
            << " COM_G=" << getCOM_G() << endl;
        o << "inertia_OB_G=" << getInertia_OB_G() << endl;
        o << "H=" << H << endl;
        o << "SVel=" << sVel << endl;
        o << "a=" << a << endl;
        o << "b=" << b << endl;
        o << "Th  =" << theta << endl;
        o << "dTh =" << dTheta << endl;
        o << "ddTh=" << ddTheta << endl;
        o << "SAcc=" << sAcc << endl;
    }
protected:
    // These are the joint-specific quantities
    //      ... position level
    FixedVector<double,dof>     theta;   // internal coordinates
    FixedMatrix<double,dof,6>   H;       // joint transition matrix (spatial)
    FixedMatrix<double,dof,dof> DI;
    FixedMatrix<double,6,dof>   G;

    //      ... velocity level
    FixedVector<double,dof>     dTheta;  // internal coordinate time derivatives

    //      ... acceleration level
    FixedVector<double,dof>     ddTheta; // - from the eq. of motion

    FixedVector<double,dof>     nu;
    FixedVector<double,dof>     epsilon;
    FixedVector<double,dof>     forceInternal;

private:
    void calcD_G(const CDSMat66& P);
};

/*static*/const double RigidBodyNode::DEG2RAD = PI / 180.;
//const double RigidBodyNode::DEG2RAD = 1.0;  //always use radians


//////////////////////////////////////////
// Derived classes for each joint type. //
//////////////////////////////////////////


/**
 * Translate (Cartesian) joint. This provides three degrees of translational freedom
 * which is suitable (e.g.) for connecting a free atom to ground.
 * The joint frame J is aligned with the body frame B.
 */
class RBNodeTranslate : public RigidBodyNodeSpec<3> {
public:
    virtual const char* type() { return "translate"; }

    RBNodeTranslate(const RBMassProperties& mProps_B,
                    int&                    nextStateOffset)
      : RigidBodyNodeSpec<3>(mProps_B,RBFrame(),nextStateOffset)
    {
    }

    void calcJointKinematicsPos() { 
        OB_P = refOrigin_P + theta;
        R_PB = ident33; // Cartesian joint can't change orientation

        // Note that this is spatial (and R_GP=R_GB for this joint)
        H = blockMat12(zero33, MatrixTools::transpose(getR_GP()));
    }

    void calcJointKinematicsVel() { 
        V_PB_G = MatrixTools::transpose(H) * dTheta;
    }
};

/**
 * This is a "pin" or "torsion" joint, meaning one degree of rotational freedom
 * about a particular axis.
 */
class RBNodeTorsion : public RigidBodyNodeSpec<1> {
public:
    virtual const char* type() { return "torsion"; }

    RBNodeTorsion(const RBMassProperties& mProps_B,
                  const RBFrame&          jointFrame,
                  int&                    nextStateOffset)
      : RigidBodyNodeSpec<1>(mProps_B,jointFrame,nextStateOffset)
    {
    }

    void calcJointKinematicsPos() { 
        OB_P = refOrigin_P; // torsion joint can't move B origin in P
        calcR_PB();
        calcH();
    }

    void calcJointKinematicsVel() { 
        V_PB_G = MatrixTools::transpose(H) * dTheta;
    }

private:
    void calcR_PB() {
        //   double scale=InternalDynamics::minimization?DEG2RAD:1.0; ??
        double scale=1.0;
        double sinTau = sin( scale * theta(0) );
        double cosTau = cos( scale * theta(0) );
        double a[] = { cosTau , -sinTau , 0.0 ,
                       sinTau ,  cosTau , 0.0 ,
                       0.0    ,  0.0    , 1.0 };
        const CDSMat33 R_JiJ(a); //rotation about z-axis

        // We need R_PB=R_PJi*R_JiJ*R_JB. But R_PJi==R_BJ, so this works:
        R_PB = orthoTransform( R_JiJ , R_BJ );
    };

    // Calc H matrix in space-fixed coords.
    void calcH() {
        // This only works because the joint z axis is the same in B & P
        // because that's what we rotate around.
        const CDSVec3 z = getR_GP() * (R_BJ * CDSVec3(0,0,1)); // R_BJ=R_PJi
        FixedMatrix<double,1,3> zMat(0.0);
        H = blockMat12( FixedMatrix<double,1,3>(z.getData()) , zMat);
    }  
};

/**
 * This class contains all the odd things required by a ball joint.
 * Any RBNode joint type which contains a ball should define a member
 * of this class and delegate to it.
 */
class ContainedBallJoint {
    CDSVec4    q; //Euler parameters for rotation relative to parent
    CDSVec4    dq;
    CDSVec4    ddq;
    double cPhi, sPhi;        //trig functions of Euler angles
    double cPsi, sPsi;        //used for minimizations
    double cTheta, sTheta;
    bool   useEuler;          //if False, use Quaternion rep.

public:
    virtual const char* type() { return "rotate3"; }

    ContainedBallJoint(int& cnt, bool shouldUseEuler)
      : q(1.0,0.0,0.0,0.0), dq(0.), ddq(0.), 
        useEuler(shouldUseEuler)
    { 
        if ( !useEuler )
            cnt++;
    }
    
    int  getBallDim() const { 
        if (useEuler) return 3; else return 4; 
    }

    void setBallPos(int stateOffset, const RVec& posv, FixedVector<double,3>& theta) {
        if (useEuler) theta = ConstRSubVec(posv,stateOffset,3).vector(); 
        else          q     = ConstRSubVec(posv,stateOffset,4).vector();
    } 

    void getBallPos(const CDSVec3& theta, int stateOffset, RVec& posv) const {
        if (useEuler) RSubVec(posv,stateOffset,3) = theta.vector();
        else          RSubVec(posv,stateOffset,4) = q.vector();
    }

    void setBallVel(int stateOffset, const RVec& velv, FixedVector<double,3>& dTheta) { 
        if (useEuler)
            dTheta = ConstRSubVec(velv,stateOffset,3).vector();
        else {
            dq = ConstRSubVec(velv,stateOffset,4).vector();
            double a2[] = {-q(1), q(0),-q(3), q(2),
                           -q(2), q(3), q(0),-q(1),
                           -q(3),-q(2), q(1), q(0)};
            FixedMatrix<double,3,4> M(a2);
            dTheta = 2.0*( M * dq );
        }
    }

    void getBallVel(const CDSVec3& dTheta, int stateOffset, RVec& velv) const {
        if (useEuler) RSubVec(velv,stateOffset,3) = dTheta.vector();
        else          RSubVec(velv,stateOffset,4) = dq.vector();
    }

    void calcBallAccel(const CDSVec3& omega, const CDSVec3& dOmega)
    {
        // called after calcAccel
        if (useEuler) return; // nothing to do here -- ddTheta is dOmega

        const double a1[] = {-q(1),-q(2),-q(3),
                              q(0), q(3),-q(2),
                             -q(3), q(0), q(1),
                              q(2),-q(1), q(0)};
        const FixedMatrix<double,4,3> M(a1);
        const double a2[] = {-dq(1),-dq(2),-dq(3),
                              dq(0), dq(3),-dq(2),
                             -dq(3), dq(0), dq(1),
                              dq(2),-dq(1), dq(0)};
        const FixedMatrix<double,4,3> dM( a2 );
        ddq = 0.5*(dM*omega + M*dOmega);
    }

    void getBallAccel(const CDSVec3& ddTheta, int stateOffset, RVec& accv) const
    {
        if (useEuler) RSubVec(accv,stateOffset,3) = ddTheta.vector();
        else          RSubVec(accv,stateOffset,4) = ddq.vector();
    }

    void calcR_PB(const CDSVec3& theta, CDSMat33& R_PB) {
        if (useEuler) {
            // theta = (Phi, Theta, Psi) Euler ``3-2-1'' angles 
            cPhi   = cos( theta(0) *RigidBodyNode::DEG2RAD );
            sPhi   = sin( theta(0) *RigidBodyNode::DEG2RAD );
            cTheta = cos( theta(1) *RigidBodyNode::DEG2RAD );
            sTheta = sin( theta(1) *RigidBodyNode::DEG2RAD );
            cPsi   = cos( theta(2) *RigidBodyNode::DEG2RAD );
            sPsi   = sin( theta(2) *RigidBodyNode::DEG2RAD );
            
            // (sherm 050726) This matches Kane's Body-three 3-2-1 sequence on page 423
            // of Spacecraft Dynamics.
            double R_JiJ[] = 
                { cPhi*cTheta , -sPhi*cPsi+cPhi*sTheta*sPsi , sPhi*sPsi+cPhi*sTheta*cPsi,
                  sPhi*cTheta ,  cPhi*cPsi+sPhi*sTheta*sPsi ,-cPhi*sPsi+sPhi*sTheta*cPsi,
                 -sTheta      ,  cTheta*sPsi                , cTheta*cPsi               };
            R_PB = CDSMat33(R_JiJ); // because P=Ji and B=J for this kind of joint
        } else {
            double R_JiJ[] =  //rotation matrix - active-sense coordinates
                {sq(q(0))+sq(q(1))-
                 sq(q(2))-sq(q(3))      , 2*(q(1)*q(2)-q(0)*q(3)), 2*(q(1)*q(3)+q(0)*q(2)),
                 2*(q(1)*q(2)+q(0)*q(3)),(sq(q(0))-sq(q(1))+
                                          sq(q(2))-sq(q(3)))     , 2*(q(2)*q(3)-q(0)*q(1)),
                 2*(q(1)*q(3)-q(0)*q(2)), 2*(q(2)*q(3)+q(0)*q(1)), (sq(q(0))-sq(q(1))-
                                                                    sq(q(2))+sq(q(3)))};
            R_PB = CDSMat33(R_JiJ); // see above
        }
    }

    void enforceBallConstraints(int offset, RVec& posv, RVec& velv) {
        if ( !useEuler ) {
            q  = RSubVec(posv,offset,4).vector();
            dq = RSubVec(velv,offset,4).vector();

            q  /= norm(q);     // Normalize Euler parameters at each time step.
            dq -= dot(q,dq)*q; // Also fix velocity: error is prop. to position component.

            RSubVec(posv,offset,4) =  q.vector();
            RSubVec(velv,offset,4) = dq.vector();
        }
    }


    void getBallInternalForce(const CDSVec3& forceInternal, int offset, RVec& v) const {
        //dependency: calcR_PB must be called first
        assert( useEuler );

        CDSVec3 torque = forceInternal;
        double a[] = { 0           , 0           , 1.0   ,
                      -sPhi        , cPhi        , 0     ,
                       cPhi*cTheta , sPhi*cTheta ,-sTheta };
        CDSMat33 M(a);
        CDSVec3 eTorque = RigidBodyNode::DEG2RAD * M * torque;

        RSubVec(v,offset,3) = eTorque.vector();
    }

    void setBallDerivs(const CDSVec3& omega) {
        assert( !useEuler );
        const double a[] = {-q(1),-q(2),-q(3),
                             q(0), q(3),-q(2),
                            -q(3), q(0), q(1),
                             q(2),-q(1), q(0)};
        const FixedMatrix<double,4,3> M(a);
        dq = 0.5*M*omega;
    } 
};

/**
 * Ball joint. This provides three degrees of rotational freedom, i.e.,
 * unrestricted orientation.
 * The joint frame J is aligned with the body frame B.
 */
class RBNodeRotate3 : public RigidBodyNodeSpec<3> {
    ContainedBallJoint ball;
public:
    virtual const char* type() { return "rotate3"; }

    RBNodeRotate3(const RBMassProperties& mProps_B,
                  int&                    nextStateOffset,
                  bool                    useEuler)
      : RigidBodyNodeSpec<3>(mProps_B,RBFrame(),nextStateOffset),
        ball(nextStateOffset,useEuler)
    {
    }
    
    int  getDim() const { return ball.getBallDim(); } 

    void setJointPos(const RVec& posv) {
        ball.setBallPos(stateOffset, posv, theta);
    } 

    void getPos(RVec& posv) const {
        ball.getBallPos(theta, stateOffset, posv);
    }

    // setPos must have been called previously
    void setJointVel(const RVec& velv) {
        ball.setBallVel(stateOffset, velv, dTheta);
    }

    void getVel(RVec& velv) const {
        ball.getBallVel(dTheta, stateOffset, velv);
    }

    void calcJointAccel() {
        ball.calcBallAccel(dTheta, ddTheta);
    }

    void getAccel(RVec& accv) const {
        ball.getBallAccel(ddTheta, stateOffset, accv);
    }

    void calcJointKinematicsPos() { 
        OB_P = refOrigin_P; // ball joint can't move B origin in P
        ball.calcR_PB(theta, R_PB);
        // H matrix in space-fixed (P) coords
        H = blockMat12(MatrixTools::transpose(getR_GP()), zero33);
    }

    // Note that dTheta = w_PB_P = ang vel of B in P, expr in P
    void calcJointKinematicsVel() { 
        V_PB_G = MatrixTools::transpose(H) * dTheta;
    }

    void enforceConstraints(RVec& posv, RVec& velv) {
        ball.enforceBallConstraints(stateOffset, posv, velv);
    }

    void getInternalForce(RVec& v) const {
        ball.getBallInternalForce(forceInternal, stateOffset, v);
    }

    void setVelFromSVel(const CDSVec6& sVel) {
        RigidBodyNodeSpec<3>::setVelFromSVel(sVel);
        ball.setBallDerivs(dTheta);
    } 
};

/**
 * Free joint. This is a six degree of freedom joint providing unrestricted 
 * translation and rotation for a free rigid body.
 * The joint frame J is aligned with the body frame B.
 */
class RBNodeTranslateRotate3 : public RigidBodyNodeSpec<6> {
    ContainedBallJoint ball;
public:
    virtual const char* type() { return "full"; }

    RBNodeTranslateRotate3(const RBMassProperties& mProps_B,
                           int&                    nextStateOffset,
                           bool                    useEuler)
      : RigidBodyNodeSpec<6>(mProps_B,RBFrame(),nextStateOffset),
        ball(nextStateOffset,useEuler)
    {
    }
    
    int  getDim() const { return ball.getBallDim() + 3; } 

    void setJointPos(const RVec& posv) {
        CDSVec3 th;
        ball.setBallPos(stateOffset, posv, th);
        RSubVec6(theta,0,3) = th.vector();
        RSubVec6(theta,3,3) = 
            ConstRSubVec(posv,stateOffset+ball.getBallDim(),3).vector();
    } 

    void getPos(RVec& posv) const {
        ball.getBallPos(CDSVec3(ConstRSubVec6(theta,0,3).vector()), stateOffset, posv);
        RSubVec(posv,stateOffset+ball.getBallDim(),3) = ConstRSubVec6(theta,3,3).vector();
    }

    // setPos must have been called previously
    void setJointVel(const RVec& velv) {
        CDSVec3 dTh;
        ball.setBallVel(stateOffset, velv, dTh);
        RSubVec6(dTheta,0,3) = dTh.vector();
        RSubVec6(dTheta,3,3) = ConstRSubVec(velv,stateOffset+ball.getBallDim(),3).vector();
    }

    void getVel(RVec& velv) const {
        ball.getBallVel(CDSVec3(ConstRSubVec6(dTheta,0,3).vector()), stateOffset, velv);
        RSubVec(velv, stateOffset+ball.getBallDim(), 3) 
            = ConstRSubVec6(dTheta,3,3).vector();
    }

    void calcJointAccel() {
        // get angular vel/accel in the space-fixed frame
        const CDSVec3 omega  = RSubVec6( dTheta, 0, 3).vector();
        const CDSVec3 dOmega = RSubVec6(ddTheta, 0, 3).vector();
        ball.calcBallAccel(omega, dOmega);
    }

    void getAccel(RVec& accv) const {
        ball.getBallAccel(CDSVec3(ConstRSubVec6(ddTheta,0,3).vector()), stateOffset, accv);
        RSubVec(accv,stateOffset+ball.getBallDim(),3) = ConstRSubVec6(ddTheta,3,3).vector();
    }

    void calcJointKinematicsPos() {
        OB_P = refOrigin_P + CDSVec3(RSubVec6(theta,3,3).vector());
        ball.calcR_PB(CDSVec3(RSubVec6(theta,0,3).vector()), R_PB);

        // H matrix in space-fixed (P) coords
        H = blockMat22( MatrixTools::transpose(getR_GP()) , zero33,
                        zero33 , MatrixTools::transpose(getR_GP()));
    }

    // Note that dTheta[0..2] = w_PB_P = ang vel of B in P, expr in P
    void calcJointKinematicsVel() { 
        V_PB_G = MatrixTools::transpose(H) * dTheta;
    }

    void enforceConstraints(RVec& posv, RVec& velv) {
        ball.enforceBallConstraints(stateOffset, posv, velv);
    }


    void getInternalForce(RVec& v) const {
        const CDSVec3 torque = ConstRSubVec6(forceInternal, 0, 3).vector();
        ball.getBallInternalForce(torque, stateOffset, v);
        RSubVec(v,stateOffset+ball.getBallDim(),3) = ConstRSubVec6(forceInternal,3,3).vector();

    }

    void setVelFromSVel(const CDSVec6& sVel) {
        RigidBodyNodeSpec<6>::setVelFromSVel(sVel);
        const CDSVec3 omega  = RSubVec6( dTheta, 0, 3).vector();
        ball.setBallDerivs(omega);
    } 
};


/**
 * U-joint like joint type which allows rotation about the two axes
 * perpendicular to zDir. This is appropriate for diatoms and for allowing 
 * torsion+bond angle bending.
 */
class RBNodeRotate2 : public RigidBodyNodeSpec<2> {
public:
    virtual const char* type() { return "rotate2"; }

    RBNodeRotate2(const RBMassProperties& mProps_B,
                  const RBFrame&          jointFrame,
                  int&                    nextStateOffset)
      : RigidBodyNodeSpec<2>(mProps_B,jointFrame,nextStateOffset)
    {
    }

    void calcJointKinematicsPos() { 
        OB_P = refOrigin_P; // no translation with this joint
        calcR_PB();
        calcH();
    }

    void calcJointKinematicsVel() { 
        V_PB_G = MatrixTools::transpose(H) * dTheta;
    }

private:
    void calcR_PB() { 
        //double scale=InternalDynamics::minimization?DEG2RAD:1.0; ??
        double scale=1.0; 
        double sinPhi = sin( scale * theta(0) );
        double cosPhi = cos( scale * theta(0) );
        double sinPsi = sin( scale * theta(1) );
        double cosPsi = cos( scale * theta(1) );

        double a[] =  //Ry(psi) * Rx(phi)
            {cosPsi , sinPsi*sinPhi , sinPsi*cosPhi,
             0      , cosPhi        , -sinPhi      ,
            -sinPsi , cosPsi*sinPhi , cosPsi*cosPhi};

        R_PB = orthoTransform( CDSMat33(a) , R_BJ );
    }

    void calcH() {
        //   double scale=InternalDynamics::minimization?DEG2RAD:1.0;
        double scale=1.0;
        const CDSMat33 tmpR_GB = getR_GP() * R_PB;

        const CDSVec3 x = scale * tmpR_GB * (R_BJ * CDSVec3(1,0,0));
        const CDSVec3 y = scale * tmpR_GB * (R_BJ * CDSVec3(0,1,0));

        const Mat23 zMat23(0.0);
        H = blockMat12(catRow23(x,y) , zMat23);
    }  
};

/**
 * The "diatom" joint is the equivalent of a free joint for a body with no inertia in
 * one direction, such as one composed of just two atoms. It allows unrestricted
 * translation but rotation only about directions perpendicular to the body's
 * inertialess axis.
 */
class RBNodeTranslateRotate2 : public RigidBodyNodeSpec<5> {
public:
    virtual const char* type() { return "diatom"; }

    RBNodeTranslateRotate2(const RBMassProperties& mProps_B,
                           const RBFrame&          jointFrame,
                           int&                    nextStateOffset)
      : RigidBodyNodeSpec<5>(mProps_B,jointFrame,nextStateOffset)
    {
    }

    void calcJointKinematicsPos() { 
        OB_P = refOrigin_P + CDSVec3(RSubVec5(theta,2,3).vector());
        calcR_PB();
        calcH();
    }

    void calcJointKinematicsVel() { 
        V_PB_G = MatrixTools::transpose(H) * dTheta;
    }

private:
    void calcR_PB() { 
        // double scale=InternalDynamics::minimization?DEG2RAD:1.0; ??
        double scale=1.0;
        double sinPhi = sin( scale * theta(0) );
        double cosPhi = cos( scale * theta(0) );
        double sinPsi = sin( scale * theta(1) );
        double cosPsi = cos( scale * theta(1) );

        // space (parent)-fixed 1-2-3 sequence (rotation 3=0)
        double R_JiJ[] =  //Ry(psi) * Rx(phi)
            {cosPsi , sinPsi*sinPhi , sinPsi*cosPhi,
             0      , cosPhi        , -sinPhi      ,
            -sinPsi , cosPsi*sinPhi , cosPsi*cosPhi};

        // calculates R0*a*R0'  (R0=R_BJ(==R_PJi), a=R_JiJ)
        R_PB = orthoTransform( CDSMat33(R_JiJ) , R_BJ ); // orientation of B in parent P
    }

    void calcH() {
        //double scale=InternalDynamics::minimization?DEG2RAD:1.0;
        double scale=1.0;
        const CDSMat33 tmpR_GB = getR_GP() * R_PB;

        CDSVec3 x = scale * tmpR_GB * (R_BJ * CDSVec3(1,0,0));
        CDSVec3 y = scale * tmpR_GB * (R_BJ * CDSVec3(0,1,0));

        const Mat23 zMat23(0.0);
        H = blockMat22(catRow23(x,y) , zMat23 ,
                        zero33       , MatrixTools::transpose(getR_GP()));
    }  
};

////////////////////////////////////////////////
// RigidBodyNode factory based on joint type. //
////////////////////////////////////////////////

/*static*/ RigidBodyNode*
RigidBodyNode::create(
    const RBMassProperties& m,            // mass properties in body frame
    const RBFrame&          jointFrame,   // inboard joint frame J in body frame
    JointType               type,
    bool                    isReversed,
    bool                    useEuler,
    int&                    nxtStateOffset)   // child-to-parent orientation?
{
    assert(!isReversed);

    switch(type) {
    case ThisIsGround:
        return new RBGroundBody();
    case TorsionJoint:
        return new RBNodeTorsion(m,jointFrame,nxtStateOffset);
    case UJoint:        
        return new RBNodeRotate2(m,jointFrame,nxtStateOffset);
    case OrientationJoint:
        return new RBNodeRotate3(m,nxtStateOffset,useEuler);
    case CartesianJoint:
        return new RBNodeTranslate(m,nxtStateOffset);
    case FreeLineJoint:
        return new RBNodeTranslateRotate2(m,jointFrame,nxtStateOffset);
    case FreeJoint:
        return new RBNodeTranslateRotate3(m,nxtStateOffset,useEuler);
    case SlidingJoint:
    case CylinderJoint:
    case PlanarJoint:
    case GimbalJoint:
    case WeldJoint:

    default: 
        assert(false);
    };

    return 0;
}

/////////////////////////////////////////////////////////////
// Implementation of RigidBodyNodeSpec base class methods. //
/////////////////////////////////////////////////////////////


//
// to be called from base to tip.
//
template<int dof> void
RigidBodyNodeSpec<dof>::setVelFromSVel(const CDSVec6& sVel) {
    dTheta = H * (sVel - transpose(phi)*parent->sVel);
}

template<int dof> void
RigidBodyNodeSpec<dof>::calcD_G(const CDSMat66& P) {
    using InternalDynamics::Exception;
    FixedMatrix<double,dof,dof> D = orthoTransform(P,H);
    try {
        DI = inverse(D);
    }
    catch ( SingularError ) {
        cerr << "calcD_G: singular D matrix: " << D << '\n'
             << "H matrix: " << H << '\n'
             << "node level: " << level << '\n'
             << "number of children: " << children.size() << '\n'
             << endl;
        throw Exception("calcD_G: singular D matrix. Bad topology?");
    }
    G = P * MatrixTools::transpose(H) * DI;
}

//
// Calculate Pk and related quantities. The requires that the children
// of the node have already had their quantities calculated, i.e. this
// is a tip to base recursion.
//
template<int dof> void
RigidBodyNodeSpec<dof>::calcP() {
    //
    //how much do we need to keep around?
    // it looks like nu and G are the only ones needed for the acceleration
    // calc. The others can be freed after the parent is done with them.
    //
    P = Mk;

    SubMatrix<CDSMat66> p11(P,0,0,3,3);
    SubMatrix<CDSMat66> p12(P,0,3,3,3);
    SubMatrix<CDSMat66> p21(P,3,0,3,3);
    SubMatrix<CDSMat66> p22(P,3,3,3,3);
    for (int i=0 ; i<children.size() ; i++) {
        // this version is readable
        // P += orthoTransform( children[i]->tau * children[i]->P ,
        //                      transpose(children[i]->phiT) );
        // this version is not
        CDSMat33 lt = crossMat(children[i]->getOB_G() - getOB_G());
        CDSMat66 M  = children[i]->tau * children[i]->P;
        SubMatrix<CDSMat66> m11(M,0,0,3,3);
        SubMatrix<CDSMat66> m12(M,0,3,3,3);
        SubMatrix<CDSMat66> m21(M,3,0,3,3);
        SubMatrix<CDSMat66> m22(M,3,3,3,3);
        p11 += m11+lt*m21-m12*lt-lt*m22*lt;
        p12 += m12+lt*m22;
        p21 += m21-m22*lt;
        p22 += m22;
    }

    calcD_G(P);
    tau.set(0.0); tau.setDiag(1.0);
    tau -= G * H;
    psiT = MatrixTools::transpose(tau) * transpose(phi);
}
 
//
// To be called from tip to base.
//
template<int dof> void
RigidBodyNodeSpec<dof>::calcZ(const CDSVec6& spatialForce) {
    z = P * a + b - spatialForce;

    for (int i=0 ; i<children.size() ; i++) 
        z += children[i]->phi
             * (children[i]->z + children[i]->Gepsilon);

    epsilon  = forceInternal - H*z;
    nu       = DI * epsilon;
    Gepsilon = G * epsilon;
}

//
// Calculate acceleration in internal coordinates, based on the last set
// of forces that were fed to calcZ (as embodied in 'nu').
// (Base to tip)
//
template<int dof> void 
RigidBodyNodeSpec<dof>::calcAccel() {
    // const Node* pNode = parentHinge.remNode;
    //make sure that this is phi is correct - FIX ME!
    // base alpha = 0!!!!!!!!!!!!!
    CDSVec6 alphap = transpose(phi) * parent->sAcc;
    ddTheta = nu - MatrixTools::transpose(G) * alphap;
    sAcc   = alphap + MatrixTools::transpose(H) * ddTheta + a;  

    calcJointAccel();   // in case joint isn't happy with just ddTheta
}

// To be called base to tip.
template<int dof> void
RigidBodyNodeSpec<dof>::calcY() {
    Y = orthoTransform(DI,MatrixTools::transpose(H))
        + orthoTransform(parent->Y,psiT);
}

//
// Calculate sum of internal force and effective forces due to Cartesian
// forces.
// To be called from tip to base.
// Should be called only once after calcProps.
//
template<int dof> void
RigidBodyNodeSpec<dof>::calcInternalForce(const CDSVec6& spatialForce) {
    z = -spatialForce;

    for (int i=0 ; i<children.size() ; i++) 
        z += children[i]->phi * children[i]->z;

    forceInternal += H * z; 
}

template<int dof> void
RigidBodyNodeSpec<dof>::print(int verbose) const {
    if (verbose&InternalDynamics::printNodePos) 
        cout << setprecision(8)
             << ": pos: " << OB_G << ' ' << '\n';
    if (verbose&InternalDynamics::printNodeTheta) 
        cout << setprecision(8)
             << ": theta: " 
             << theta << ' ' << dTheta  << ' ' << ddTheta  << '\n';
}

/////////////////////////////////////
// Miscellaneous utility routines. //
/////////////////////////////////////

static Mat23
catRow23(const CDSVec3& v1, const CDSVec3& v2) {
    FixedMatrix<double,1,3> m1(v1.getData());
    FixedMatrix<double,1,3> m2(v2.getData());
    Mat23 ret = blockMat21(m1,m2);
    return ret;
}

// Calculate a rotation matrix R_BJ which defines the J
// frame by taking the B frame z axis into alignment 
// with the passed-in zDir vector. This is not unique.
// notes of 12/6/99 - CDS
static CDSMat33
makeJointFrameFromZAxis(const CDSVec3& zVec) {
    const CDSVec3 zDir = unitVec(zVec);

    // Calculate spherical coordinates.
    double theta = acos( zDir.z() );             // zenith (90-elevation)
    double psi   = atan2( zDir.x() , zDir.y() ); // 90-azimuth

    // This is a space fixed 1-2-3 sequence with angles
    // a1=-theta, a2=0, a3=-psi. That is, to get from B to J
    // first rotate by -theta around the B frame x axis, 
    // then rotate by -psi around the B frame z axis. (sherm)

    const double R_BJ[] = 
        { cos(psi) , cos(theta)*sin(psi) , sin(psi)*sin(theta),
         -sin(psi) , cos(theta)*cos(psi) , cos(psi)*sin(theta),
          0        , -sin(theta)         , cos(theta)         };
    return CDSMat33(R_BJ); // == R_PJi
}

static CDSMat33
makeIdentity33() {
    CDSMat33 ret(0.);
    ret.setDiag(1.);
    return ret;
}

