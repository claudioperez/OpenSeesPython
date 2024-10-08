///////////////////////////////////////////////////////////////////////////////
// Description: This file contains the class definition for                  //
// NineFourNodeQuadUP, a 9-4-node (9 node for solid and 4 node for fluid) //
// plane strain element for solid-fluid fully coupled analysis. This         //
// implementation is a simplified u-p formulation of Biot theory             //
// (u - solid displacement, p - fluid pressure). Each element node has two   //
// DOFs for u and 1 DOF for p.                                               //
//                                                                           //
// Written by Zhaohui Yang	(March 2004)                                     //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

// $Revision: 1.3 $
// $Date: 2010-02-04 19:14:35 $
// $Source: /usr/local/cvs/OpenSees/SRC/element/UP-ucsd/Nine_Four_Node_QuadUPOld.cpp,v $

#include <Nine_Four_Node_QuadUP.h>
#include <Node.h>
#include <NDMaterial.h>
#include <Matrix.h>
#include <Vector.h>
#include <ID.h>
#include <Renderer.h>
#include <Domain.h>
#include <string.h>
#include <Information.h>
#include <Channel.h>
#include <FEM_ObjectBroker.h>
#include <ElementResponse.h>

Matrix NineFourNodeQuadUP::K(22,22);
Vector NineFourNodeQuadUP::P(22);
double NineFourNodeQuadUP::shgu[3][9][9];	
double NineFourNodeQuadUP::shgp[3][4][4];	
double NineFourNodeQuadUP::shgq[3][9][4];	
double NineFourNodeQuadUP::shlu[3][9][9];	
double NineFourNodeQuadUP::shlp[3][4][4];	
double NineFourNodeQuadUP::shlq[3][9][4];	
double NineFourNodeQuadUP::wu[9];	
double NineFourNodeQuadUP::wp[4];	
double NineFourNodeQuadUP::dvolu[9];  
double NineFourNodeQuadUP::dvolp[4];  
double NineFourNodeQuadUP::dvolq[4];  
const int NineFourNodeQuadUP::nintu=9;
const int NineFourNodeQuadUP::nintp=4;
const int NineFourNodeQuadUP::nenu=9;
const int NineFourNodeQuadUP::nenp=4;
 
NineFourNodeQuadUP::NineFourNodeQuadUP(int tag, 
	int nd1, int nd2, int nd3, int nd4,int nd5, int nd6, int nd7, int nd8,int nd9,
	NDMaterial &m, const char *type, double t, double bulk, double r,
		  double p1, double p2, double b1, double b2)
:Element (tag, ELE_TAG_Nine_Four_Node_QuadUP), 
  theMaterial(0), connectedExternalNodes(9), 
  Ki(0), Q(22), thickness(t), kc(bulk), rho(r)
{
    this->shapeFunction(wu, nintu, nenu, 0);
    this->shapeFunction(wp, nintp, nenp, 1);
    this->shapeFunction(wp, nintp, nenu, 2);

	// Body forces
	b[0] = b1;
	b[1] = b2;
	// Permeabilities
    perm[0] = p1;
    perm[1] = p2;

    // Allocate arrays of pointers to NDMaterials
    theMaterial = new NDMaterial *[nintu];
    
    if (theMaterial == 0) {
      opserr << "NineFourNodeQuadUP::NineFourNodeQuadUP - failed allocate material model pointer\n";
    }

    for (int i = 0; i < nintu; i++) {
      
      // Get copies of the material model for each integration point
      theMaterial[i] = m.getCopy(type);
      
      // Check allocation
      if (theMaterial[i] == 0) {
	     opserr << "NineFourNodeQuadUP::NineFourNodeQuadUP -- failed to get a copy of material model\n";
	     exit(-1);
      }
    }

    // Set connected external node IDs
    connectedExternalNodes(0) = nd1;
    connectedExternalNodes(1) = nd2;
    connectedExternalNodes(2) = nd3;
    connectedExternalNodes(3) = nd4;
    connectedExternalNodes(4) = nd5;
    connectedExternalNodes(5) = nd6;
    connectedExternalNodes(6) = nd7;
    connectedExternalNodes(7) = nd8;
    connectedExternalNodes(8) = nd9;
}
 

NineFourNodeQuadUP::NineFourNodeQuadUP()
:Element (0,ELE_TAG_Nine_Four_Node_QuadUP),  
  theMaterial(0), connectedExternalNodes(9), 
  Ki(0), Q(22), thickness(0.0), kc(0.0), rho(0.0)
{
    this->shapeFunction(wu, nintu, nenu, 0);
    this->shapeFunction(wp, nintp, nenp, 1);
    this->shapeFunction(wp, nintp, nenu, 2);
}

NineFourNodeQuadUP::~NineFourNodeQuadUP()
{    
    for (int i = 0; i < nintu; i++) {
      if (theMaterial[i])
	delete theMaterial[i];
    }

    // Delete the array of pointers to NDMaterial pointer arrays
    if (theMaterial)
		delete [] theMaterial;

    for (int ii = 0; ii < nenu; ii++) theNodes[ii] = 0 ;

    if (Ki != 0)
      delete Ki;
}

int
NineFourNodeQuadUP::getNumExternalNodes() const
{
    return nenu;
}

const ID&
NineFourNodeQuadUP::getExternalNodes()
{
    return connectedExternalNodes;
}

Node **
NineFourNodeQuadUP::getNodePtrs()
{
  return theNodes;
}

int
NineFourNodeQuadUP::getNumDOF()
{
    return 22;
}

void
NineFourNodeQuadUP::setDomain(Domain *theDomain)
{
  // Check Domain is not null - invoked when object removed from a domain
  if (theDomain == 0) {
    for (int i=0; i<nenu; i++) theNodes[i] = 0;
    return;
  }
  
  int i;
  for (int i=0; i<nenu; i++) {
    theNodes[i] = theDomain->getNode(connectedExternalNodes(i));
    if (theNodes[i] == nullptr) {
      opserr << "FATAL ERROR NineFourNodeQuadUP, node not found in domain, tag "
	     << this->getTag();
      return;
    }
  }

  for (int i=0; i<nenu; i++) {
    int dof = theNodes[i]->getNumberDOF();
    if ((i<nenp && dof != 3) || (i>=nenp && dof != 2)) {
      opserr << "FATAL ERROR NineFourNodeQuadUP, has wrong number of DOFs at its nodes "
	     << this->getTag();
      return;
		 }
  }
  
  this->DomainComponent::setDomain(theDomain);
}


int
NineFourNodeQuadUP::commitState()
{
    int retVal = 0;

    // call element commitState to do any base class stuff
    if ((retVal = this->Element::commitState()) != 0) {
      opserr << "Nine_Four_Node_Quad_UP::commitState () - failed in base class";
    }    

    // Loop over the integration points and commit the material states
    for (int i = 0; i < nintu; i++)
      retVal += theMaterial[i]->commitState();

    return retVal;
}

int
NineFourNodeQuadUP::revertToLastCommit()
{
    int retVal = 0;

    // Loop over the integration points and revert to last committed state
    for (int i = 0; i < nintu; i++)
		retVal += theMaterial[i]->revertToLastCommit();

    return retVal;
}

int
NineFourNodeQuadUP::revertToStart()
{
    int retVal = 0;

    // Loop over the integration points and revert states to start
    for (int i = 0; i < nintu; i++)
		retVal += theMaterial[i]->revertToStart();

    return retVal;
}

int
NineFourNodeQuadUP::update()
{
  static double u[2][9];
  int i;
  for (i = 0; i < nenu; i++) {
    const Vector &disp = theNodes[i]->getTrialDisp();
    u[0][i] = disp(0);
    u[1][i] = disp(1);
  }
  
  static Vector eps(3);
  
  int ret = 0;
  
  // Determine Jacobian for this integration point
  this->globalShapeFunction(dvolu, wu, nintu, nenu, 0); 

  // Loop over the integration points
  for (i = 0; i < nintu; i++) {
    
    // Interpolate strains
    //eps = B*u;
    //eps.addMatrixVector(0.0, B, u, 1.0);
    eps.Zero();
    for (int beta = 0; beta < nenu; beta++) {
      eps(0) += shgu[0][beta][i]*u[0][beta];
      eps(1) += shgu[1][beta][i]*u[1][beta];
      eps(2) += shgu[0][beta][i]*u[1][beta] + shgu[1][beta][i]*u[0][beta];
    }
    
    // Set the material strain
    ret += theMaterial[i]->setTrialStrain(eps);
  }
  
  return ret;
}


const Matrix&
NineFourNodeQuadUP::getTangentStiff()
{
  static Matrix B(3,nenu*2);
  static Matrix BTDB(nenu*2,nenu*2);

  B.Zero(); 
  BTDB.Zero();
  K.Zero();

  // Determine Jacobian for this integration point
  this->globalShapeFunction(dvolu, wu, nintu, nenu, 0); 
  
  // Loop over the integration points
  for (int i = 0; i < nintu; i++) {
    
    int j, j2, j2m1, ik, ib, jk, jb;
    // Get the material tangent
    const Matrix &D = theMaterial[i]->getTangent();
    
	for (int j=0; j<nenu; j++) {
		j2 = j*2+1;
		j2m1 = j*2;
        B(0,j2m1) = shgu[0][j][i];
		B(0,j2)   = 0.;
		B(1,j2m1) = 0.;
		B(1,j2)   = shgu[1][j][i];
		B(2,j2m1) = shgu[1][j][i];
		B(2,j2)   = shgu[0][j][i];
    }

    // Perform numerical integration
    //K = K + (B^ D * B) * intWt(i)*intWt(j) * detJ;
    BTDB.addMatrixTripleProduct(1.0, B, D, dvolu[i]);
  }

  for (int i = 0; i < nenu; i++) {
	  if (i<nenp) ik = i*3;
      if (i>=nenp) ik = nenp*3 + (i-nenp)*2;
      ib = i*2;

	  for (int j = 0; j < nenu; j++) {
		  if (j<nenp) jk = j*3;
		  if (j>=nenp) jk = nenp*3 + (j-nenp)*2;
          jb = j*2;

          K(ik,jk) += BTDB(ib,jb);
		  K(ik+1,jk) += BTDB(ib+1,jb);
		  K(ik,jk+1) += BTDB(ib,jb+1);
		  K(ik+1,jk+1) += BTDB(ib+1,jb+1);
	  }
  }

  return K;
}


const Matrix &NineFourNodeQuadUP::getInitialStiff () 
{
  if (Ki != 0) 
    return *Ki;

  int i, j, j2, j2m1, ik, ib, jk, jb;
  static Matrix B(3,nenu*2);
  static Matrix BTDB(nenu*2,nenu*2);

  B.Zero(); 
  BTDB.Zero();
  K.Zero();

  // Determine Jacobian for this integration point
  this->globalShapeFunction(dvolu, wu, nintu, nenu, 0); 
  
  // Loop over the integration points
  for (i = 0; i < nintu; i++) {
    
    // Get the material tangent
    const Matrix &D = theMaterial[i]->getInitialTangent();
    
	for (j=0; j<nenu; j++) {
		j2 = j*2+1;
		j2m1 = j*2;
        B(0,j2m1) = shgu[0][j][i];
		B(0,j2)   = 0.;
		B(1,j2m1) = 0.;
		B(1,j2)   = shgu[1][j][i];
		B(2,j2m1) = shgu[1][j][i];
		B(2,j2)   = shgu[0][j][i];
    }

    // Perform numerical integration
    //K = K + (B^ D * B) * intWt(i)*intWt(j) * detJ;
    BTDB.addMatrixTripleProduct(1.0, B, D, dvolu[i]);
  }

  for (i = 0; i < nenu; i++) {
	  if (i<nenp) ik = i*3;
      if (i>=nenp) ik = nenp*3 + (i-nenp)*2;
      ib = i*2;

	  for (j = 0; j < nenu; j++) {
		  if (j<nenp) jk = j*3;
		  if (j>=nenp) jk = nenp*3 + (j-nenp)*2;
          jb = j*2;
          K(ik,jk) += BTDB(ib,jb);
		  K(ik+1,jk) += BTDB(ib+1,jb);
		  K(ik,jk+1) += BTDB(ib,jb+1);
		  K(ik+1,jk+1) += BTDB(ib+1,jb+1);
	  }
  }

  Ki = new Matrix(K);
  if (Ki == 0) {
    opserr << "FATAL NineFourNodeQuadUP::getInitialStiff() -";
    opserr << "ran out of memory\n";
    exit(-1);
  }  
    
  return *Ki;
}


const Matrix&
NineFourNodeQuadUP::getDamp()
{
  static Matrix Kdamp(22,22);
  Kdamp.Zero();
  
  if (betaK != 0.0)
    Kdamp.addMatrix(1.0, this->getTangentStiff(), betaK);      
  if (betaK0 != 0.0)
    Kdamp.addMatrix(1.0, this->getInitialStiff(), betaK0);      
  if (betaKc != 0.0)
    Kdamp.addMatrix(1.0, *Kc, betaKc);      

  int i, j, m, ik, jk;

  if (alphaM != 0.0) {
	 this->getMass();

     for (i = 0; i < nenu; i++) {
	     if (i<nenp) ik = i*3;
         if (i>=nenp) ik = nenp*3 + (i-nenp)*2;

	     for (j = 0; j < nenu; j++) {
		     if (j<nenp) jk = j*3;
		     if (j>=nenp) jk = nenp*3 + (j-nenp)*2;

             Kdamp(ik,jk) += K(ik,jk)*alphaM;
             Kdamp(ik+1,jk+1) += K(ik+1,jk+1)*alphaM;
		 }
     }  
  }

  // Determine Jacobian for this integration point
  this->globalShapeFunction(dvolq, wp, nintp, nenu, 2); 
  this->globalShapeFunction(dvolp, wp, nintp, nenp, 1); 

  // Compute coupling matrix
  for (i = 0; i < nenu; i++) {
	   if (i<nenp) ik = i*3;
       if (i>=nenp) ik = nenp*3 + (i-nenp)*2;

       for (j = 0; j < nenp; j++) {
	       jk = j*3+2;

           for (m = 0; m < nintp; m++) {
	           Kdamp(ik,jk) += -dvolq[m]*shgq[0][i][m]*shgp[2][j][m];
	           Kdamp(ik+1,jk) += -dvolq[m]*shgq[1][i][m]*shgp[2][j][m];
		   }
           Kdamp(jk,ik) = Kdamp(ik,jk);
           Kdamp(jk,ik+1) = Kdamp(ik+1,jk);
	   }
  }
  
  // Compute permeability matrix
  for (i = 0; i < nenp; i++) {
       ik = i*3+2;

       for (j = 0; j < nenp; j++) {
	       jk = j*3+2;

           for (m = 0; m < nintp; m++) {
    	       Kdamp(ik,jk) += - dvolp[m]*(perm[0]*shgp[0][i][m]*shgp[0][j][m] +
	                           perm[1]*shgp[1][i][m]*shgp[1][j][m]);
		   }
	   }
  }

  K = Kdamp;
  return K;
}

const Matrix&
NineFourNodeQuadUP::getMass()
{
  K.Zero();
  
  int i, j, m, ik, jk;
  double Nrho;
  
  // Determine Jacobian for this integration point
  this->globalShapeFunction(dvolu, wu, nintu, nenu, 0); 

  // Compute consistent mass matrix
  for (i = 0; i < nenu; i++) {
	  if (i<nenp) ik = i*3;
      if (i>=nenp) ik = nenp*3 + (i-nenp)*2;

	  for (j = 0; j < nenu; j++) {
		  if (j<nenp) jk = j*3;
	      if (j>=nenp) jk = nenp*3 + (j-nenp)*2;

          for (m = 0; m < nintu; m++) {
              Nrho = dvolu[m]*mixtureRho(m)*shgu[2][i][m]*shgu[2][j][m];
              K(ik,jk) += Nrho;
              K(ik+1,jk+1) += Nrho;
		  }
	  }
  }
  
  // Compute compressibility matrix
  double oneOverKc = 1./kc;
  this->globalShapeFunction(dvolp, wp, nintp, nenp, 1); 
  
  for (i = 0; i < nenp; i++) {
       ik = i*3+2;

       for (j = 0; j < nenp; j++) {
	       jk = j*3+2;

           for (m = 0; m < nintp; m++) {
	          K(ik,jk) += -dvolp[m]*oneOverKc*shgp[2][i][m]*shgp[2][j][m];
	  }
    }
  }

  return K;
}

void
NineFourNodeQuadUP::zeroLoad(void)
{
	Q.Zero();

	return;
}

int 
NineFourNodeQuadUP::addLoad(ElementalLoad *theLoad, double loadFactor)
{
  opserr << "NineFourNodeQuadUP::addLoad - load type unknown for ele with tag: " << this->getTag() << "\n";
  return -1;
}


int 
NineFourNodeQuadUP::addInertiaLoadToUnbalance(const Vector &accel)
{
  // accel = uDotDotG (see EarthquakePattern.cpp)
  // Get R * accel from the nodes

  static Vector ra(22);
  int i, j, ik;

  ra.Zero();

  for (i=0; i<nenu; i++) {
      const Vector &Raccel = theNodes[i]->getRV(accel);
      if ((i<nenp && 3 != Raccel.Size()) || (i>=nenp && 2 != Raccel.Size())) {
         opserr << "NineFourNodeQuadUP::addInertiaLoadToUnbalance matrix and vector sizes are incompatible\n";
         return -1;
	  }

  	  if (i<nenp) ik = i*3;
      if (i>=nenp) ik = nenp*3 + (i-nenp)*2;
      ra[ik] = Raccel(0);
      ra[ik+1] = Raccel(1);
  }

  // Compute mass matrix
  this->getMass();
  
  // Want to add ( - fact * M R * accel ) to unbalance 
  for (i = 0; i < 22; i++) {
    for (j = 0; j < 22; j++)
      Q(i) += -K(i,j)*ra[j];
  }
  
  return 0;
}

const Vector&
NineFourNodeQuadUP::getResistingForce()
{
  P.Zero();

  // Determine Jacobian for this integration point
  this->globalShapeFunction(dvolu, wu, nintu, nenu, 0); 
  this->globalShapeFunction(dvolp, wp, nintp, nenp, 1); 
  
  int i, j, jk;

  // Loop over the integration points
  for (i = 0; i < nintu; i++) {

    // Get material stress response
    const Vector &sigma = theMaterial[i]->getStress();
    
    // Perform numerical integration on internal force
    //P = P + (B^ sigma) * intWt(i)*intWt(j) * detJ;
    //P.addMatrixTransposeVector(1.0, B, sigma, intWt(i)*intWt(j)*detJ);
	for (int j = 0; j < nenu; j++) {
		if (j<nenp) jk = j*3;
	    if (j>=nenp) jk = nenp*3 + (j-nenp)*2;

        P(jk) += dvolu[i]*(shgu[0][j][i]*sigma(0) + shgu[1][j][i]*sigma(2));      
        P(jk+1) += dvolu[i]*(shgu[1][j][i]*sigma(1) + shgu[0][j][i]*sigma(2));
      
        // Subtract equiv. body forces from the nodes
        //P = P - (N^ b) * intWt(i)*intWt(j) * detJ;
        //P.addMatrixTransposeVector(1.0, N, b, -intWt(i)*intWt(j)*detJ);
        double r = mixtureRho(i);
        P(jk) -= dvolu[i]*(shgu[2][j][i]*r*b[0]);
        P(jk+1) -= dvolu[i]*(shgu[2][j][i]*r*b[1]);
    }
  }
//  opserr<<"K -body"<<P<<endln;
  
  // Subtract fluid body force
  for (j = 0; j < nenp; j++) {
	 jk = j*3+2;
     for (i = 0; i < nintp; i++) {
        P(jk) += dvolp[i]*rho*(perm[0]*b[0]*shgp[0][j][i] +
                 perm[1]*b[1]*shgp[1][j][i]);
     }
  }

//  opserr<<"K -fluid "<<P<<endln;

  // Subtract other external nodal loads ... P_res = P_int - P_ext
  //P = P - Q;
  P.addVector(1.0, Q, -1.0);
  
  return P;
}

const Vector&
NineFourNodeQuadUP::getResistingForceIncInertia()
{
  int i, j, ik;
  static double a[22];

  for (i=0; i<nenu; i++) {
    const Vector &accel = theNodes[i]->getTrialAccel();
    if ((i<nenp && 3 != accel.Size()) || (i>=nenp && 2 != accel.Size())) {
      opserr << "NineFourNodeQuadUP::getResistingForceIncInertia matrix and vector sizes are incompatible\n";
         return P;
    }
    
    if (i<nenp) ik = i*3;
    if (i>=nenp) ik = nenp*3 + (i-nenp)*2;
    a[ik] = accel(0);
      a[ik+1] = accel(1);
      if (i<nenp) a[ik+2] = accel(2);
  }
  
  // Compute the current resisting force
  this->getResistingForce();
  //  opserr<<"K "<<P<<endln;
  
  // Compute the mass matrix
  this->getMass();
  
  for (i = 0; i < 22; i++) {
    for (j = 0; j < 22; j++)
      P(i) += K(i,j)*a[j];
  }
//  opserr<<"K+M "<<P<<endln; 
   
  for (i=0; i<nenu; i++) {
      const Vector &vel = theNodes[i]->getTrialVel();
      if ((i<nenp && 3 != vel.Size()) || (i>=nenp && 2 != vel.Size())) {
         opserr << "NineFourNodeQuadUP::getResistingForceIncInertia matrix and vector sizes are incompatible\n";
         return P;
      }

      if (i<nenp) ik = i*3;
      if (i>=nenp) ik = nenp*3 + (i-nenp)*2;
      a[ik] = vel(0);
      a[ik+1] = vel(1);
      if (i<nenp) a[ik+2] = vel(2);
  }
  
  this->getDamp();
  
  for (i = 0; i < 22; i++) {
    for (j = 0; j < 22; j++) {
      P(i) += K(i,j)*a[j];
    }
  }
//  opserr<<"final "<<P<<endln;
  return P;
}

int
NineFourNodeQuadUP::sendSelf(int commitTag, Channel &theChannel)
{
  int res = 0;
  
  // note: we don't check for dataTag == 0 for Element
  // objects as that is taken care of in a commit by the Domain
  // object - don't want to have to do the check if sending data
  int dataTag = this->getDbTag();
  
  // Quad packs its data into a Vector and sends this to theChannel
	// along with its dbTag and the commitTag passed in the arguments
  static Vector data(10);
  data(0) = this->getTag();
  data(1) = thickness;
  data(2) = rho;
  data(3) = b[0];
  data(4) = b[1];
  //data(5) = pressure;
  data(6) = kc;
  data(7) = perm[0];
  data(8) = perm[1];
  data(9) = 9;
  
  
  res += theChannel.sendVector(dataTag, commitTag, data);
  if (res < 0) {
    opserr << "WARNING NineFourNodeQuadUP::sendSelf() - " << this->getTag() << " failed to send Vector\n";
    return res;
  }	      
  
  // Quad then sends the tags of its four end nodes
  res += theChannel.sendID(dataTag, commitTag, connectedExternalNodes);
  if (res < 0) {
    opserr << "WARNING NineFourNodeQuadUP::sendSelf() - " << this->getTag() << " failed to send ID\n";
    return res;
  }
  
  // Now quad sends the ids of its materials
  int matDbTag;
  int numMats = 9;
  ID classTags(2*numMats);
  
  int i;
  for (i = 0; i < 9; i++) {
    classTags(i) = theMaterial[i]->getClassTag();
    matDbTag = theMaterial[i]->getDbTag();
    // NOTE: we do have to ensure that the material has a database
    // tag if we are sending to a database channel.
    if (matDbTag == 0) {
      matDbTag = theChannel.getDbTag();
      if (matDbTag != 0)
	theMaterial[i]->setDbTag(matDbTag);
    }
    classTags(i+numMats) = matDbTag;
  }
  
  res += theChannel.sendID(dataTag, commitTag, classTags);
  if (res < 0) {
    opserr << "WARNING NineFourNodeQuadUP::sendSelf() - " << this->getTag() << " failed to send ID\n";
    return res;
  }
  
  // Finally, quad asks its material objects to send themselves
  for (i = 0; i < 9; i++) {
    res += theMaterial[i]->sendSelf(commitTag, theChannel);
    if (res < 0) {
      opserr << "WARNING NineFourNodeQuadUP::sendSelf() - " << this->getTag() << " failed to send its Material\n";
      return res;
    }
  }
  
  return res;
}

int
NineFourNodeQuadUP::recvSelf(int commitTag, Channel &theChannel,
						FEM_ObjectBroker &theBroker)
{
  int res = 0;
  
  int dataTag = this->getDbTag();
  
  // Quad creates a Vector, receives the Vector and then sets the 
  // internal data with the data in the Vector
  static Vector data(7);
  res += theChannel.recvVector(dataTag, commitTag, data);
  if (res < 0) {
    opserr << "WARNING NineFourNodeQuadUP::recvSelf() - failed to receive Vector\n";
    return res;
  }
  
  this->setTag((int)data(0));
  thickness = data(1);
  rho = data(2);
  b[0] = data(3);
  b[1] = data(4);
  //pressure = data(5);
  kc = data(6);
  perm[0] = data(7);
  perm[1] = data(8);

  // Quad now receives the tags of its four external nodes
  res += theChannel.recvID(dataTag, commitTag, connectedExternalNodes);
  if (res < 0) {
    opserr << "WARNING NineFourNodeQuadUP::recvSelf() - " << this->getTag() << " failed to receive ID\n";
    return res;
  }

  // Quad now receives the ids of its materials
  int newOrder = (int)data(9);
  int numMats = newOrder;
  ID classTags(2*numMats);

  res += theChannel.recvID(dataTag, commitTag, classTags);
  if (res < 0)  {
    opserr << "NineFourNodeQuadUP::recvSelf() - failed to recv ID data\n";
    return res;
  }    

  int i;
  
  // If the number of materials (quadrature order) is not the same,
  // delete the old materials, allocate new ones and then receive
  if (9 != newOrder) {
		// Delete the materials
    for (i = 0; i < 9; i++) {
      if (theMaterial[i])
	delete theMaterial[i];
		}
    if (theMaterial)
      delete [] theMaterial;
    
    // Allocate new materials
    theMaterial = new NDMaterial *[9];
    if (theMaterial == 0) {
      opserr << "NineFourNodeQuadUP::recvSelf() - Could not allocate NDMaterial* array\n";
      return -1;
    }
    for (i = 0; i < 9; i++) {
      int matClassTag = classTags(i);
      int matDbTag = classTags(i+numMats);
      // Allocate new material with the sent class tag
      theMaterial[i] = theBroker.getNewNDMaterial(matClassTag);
      if (theMaterial[i] == 0) {
	opserr << "NineFourNodeQuadUP::recvSelf() - Broker could not create NDMaterial of class type" << matClassTag << endln;
	return -1;
      }
      // Now receive materials into the newly allocated space
      theMaterial[i]->setDbTag(matDbTag);
      res += theMaterial[i]->recvSelf(commitTag, theChannel, theBroker);
      if (res < 0) {
	opserr << "NLBeamColumn3d::recvSelf() - material " << i << "failed to recv itself\n";
	return res;
      }
    }
  }
  // Number of materials is the same, receive materials into current space
  else {
    for (i = 0; i < 9; i++) {
      int matClassTag = classTags(i);
      int matDbTag = classTags(i+numMats);
      // Check that material is of the right type; if not,
      // delete it and create a new one of the right type
      if (theMaterial[i]->getClassTag() != matClassTag) {
	delete theMaterial[i];
	theMaterial[i] = theBroker.getNewNDMaterial(matClassTag);
	if (theMaterial[i] == 0) {
	  opserr << "NineFourNodeQuadUP::recvSelf() - Broker could not create NDMaterial of class type " << matClassTag << endln;
	  exit(-1);
	}
      }
      // Receive the material
      theMaterial[i]->setDbTag(matDbTag);
      res += theMaterial[i]->recvSelf(commitTag, theChannel, theBroker);
      if (res < 0) {
	opserr << "NineFourNodeQuadUP::recvSelf() - material " << i << "failed to recv itself\n";
	return res;
      }
    }
  }
  
  return res;
}

void
NineFourNodeQuadUP::Print(OPS_Stream &s, int flag)
{
	s << "\nNineFourNodeQuadUP, element id:  " << this->getTag() << endln;
	s << "\tConnected external nodes:  " << connectedExternalNodes;
	s << "\tthickness:  " << thickness << endln;
	s << "\tmass density:  " << rho << endln;
	//s << "\tsurface pressure:  " << pressure << endln;
	s << "\tbody forces:  " << b[0] << ' ' << b[1] << endln;
	theMaterial[0]->Print(s,flag);
	s << "\tStress (xx yy xy)" << endln;
	for (int i = 0; i < 9; i++)
		s << "\t\tGauss point " << i+1 << ": " << theMaterial[i]->getStress();
}

int
NineFourNodeQuadUP::displaySelf(Renderer &theViewer, int displayMode, float fact)
{
    // first set the quantity to be displayed at the nodes;
    // if displayMode is 1 through 3 we will plot material stresses otherwise 0.0

    static Vector values(9);

    for (int j=0; j<9; j++)
	   values(j) = 0.0;

    if (displayMode < 4 && displayMode > 0) {
	for (int i=0; i<9; i++) {
	  const Vector &stress = theMaterial[i]->getStress();
	  values(i) = stress(displayMode-1);
	}
    }

    // now  determine the end points of the quad based on
    // the display factor (a measure of the distorted image)
    // store this information in 4 3d vectors v1 through v4
    /*const Vector &end1Crd = nd1Ptr->getCrds();
    const Vector &end2Crd = nd2Ptr->getCrds();	
    const Vector &end3Crd = nd3Ptr->getCrds();	
    const Vector &end4Crd = nd4Ptr->getCrds();	

    const Vector &end1Disp = nd1Ptr->getDisp();
    const Vector &end2Disp = nd2Ptr->getDisp();
    const Vector &end3Disp = nd3Ptr->getDisp();
    const Vector &end4Disp = nd4Ptr->getDisp();

    static Matrix coords(4,3);

    for (int i = 0; i < 2; i++) {
      coords(0,i) = end1Crd(i) + end1Disp(i)*fact;
      coords(1,i) = end2Crd(i) + end2Disp(i)*fact;    
      coords(2,i) = end3Crd(i) + end3Disp(i)*fact;    
      coords(3,i) = end4Crd(i) + end4Disp(i)*fact;    
    }

    int error = 0;

    // finally we draw the element using drawPolygon
    error += theViewer.drawPolygon (coords, values);

    return error;*/
	return 0;
}

Response*
NineFourNodeQuadUP::setResponse(const char **argv, int argc)
{
    if (strcmp(argv[0],"force") == 0 || strcmp(argv[0],"forces") == 0)
		return new ElementResponse(this, 1, P);
    
    else if (strcmp(argv[0],"stiff") == 0 || strcmp(argv[0],"stiffness") == 0)
		return new ElementResponse(this, 2, K);

    else if (strcmp(argv[0],"mass") == 0)
		return new ElementResponse(this, 3, K);

    else if (strcmp(argv[0],"damp") == 0)
		return new ElementResponse(this, 4, K);

	else if (strcmp(argv[0],"material") == 0 || strcmp(argv[0],"integrPoint") == 0) {
		int pointNum = atoi(argv[1]);
		if (pointNum > 0 && pointNum <= nenu)
			return theMaterial[pointNum-1]->setResponse(&argv[2], argc-2);
	    else 
			return 0;
	}
 
    // otherwise response quantity is unknown for the quad class
    else
		return 0;
}

int 
NineFourNodeQuadUP::getResponse(int responseID, Information &eleInfo)
{
	switch (responseID) {
      
		case 1:
			return eleInfo.setVector(this->getResistingForce());
      
		case 2:
			return eleInfo.setMatrix(this->getTangentStiff());

		case 3:
			return eleInfo.setMatrix(this->getMass());

		case 4:
			return eleInfo.setMatrix(this->getDamp());

		default: 
			return -1;
	}
}

int
NineFourNodeQuadUP::setParameter(const char **argv, int argc, Information &info)
{
	// quad mass density per unit volume
	if (strcmp(argv[0],"rho") == 0) {
		info.theType = DoubleType;
		info.theDouble = rho;
		return 1;
	}

    // a material parameter
    else if (strcmp(argv[0],"material") == 0) {
		int pointNum = atoi(argv[1]);
		if (pointNum > 0 && pointNum <= nenu) {
			int ok = theMaterial[pointNum-1]->setParameter(&argv[2], argc-2, info);
			if (ok < 0)
				return -1;
		    else if (ok >= 0 && ok < 100)
				return pointNum*100 + ok;
			else
				return -1;
		}
	    else 
			return -1;
	}
    
    // otherwise parameter is unknown for the Truss class
    else
		return -1;

}
    
int
NineFourNodeQuadUP::updateParameter(int parameterID, Information &info)
{
  switch (parameterID) {
    case -1:
      return -1;
      
	case 1:
		rho = info.theDouble;
		this->getMass();	// update mass matrix
		return 0;

	default: 
		if (parameterID >= 100) { // material parameter
			int pointNum = parameterID/100;
			if (pointNum > 0 && pointNum <= nenu)
				return theMaterial[pointNum-1]->updateParameter(parameterID-100*pointNum, info);
			else
				return -1;
		} else // unknown
			return -1;
  }
}

void NineFourNodeQuadUP::globalShapeFunction(double *dvol, double *w, int nint, int nen, int mode)
{
  static double coord[2][9], xs[2][2], det, temp;
  int i, j, k, m;

  for (i=0; i<3; i++) {
     for (j=0; j<nen; j++) {
        for (k=0; k<nint; k++) {
           if (mode==0) shgu[i][j][k] = shlu[i][j][k];
           if (mode==1) shgp[i][j][k] = shlp[i][j][k];
           if (mode==2) shgq[i][j][k] = shlq[i][j][k];
		}
	 }
  }

  for (i=0; i<nen; i++) {
	 const Vector &coordRef = theNodes[i]->getCrds();
	 coord[0][i] = coordRef(0);
	 coord[1][i] = coordRef(1);
  }

  for (m=0; m<nint; m++) {
      for (i=0; i<2; i++) {
	      for (j=0; j<2; j++) {
	           xs[i][j] = 0.0;
	           for (k=0; k<nen; k++) {
                    if (mode==0) xs[i][j] += coord[j][k]*shgu[i][k][m]; 
                    if (mode==1) xs[i][j] += coord[j][k]*shgp[i][k][m]; 
                    if (mode==2) xs[i][j] += coord[j][k]*shgq[i][k][m]; 
               }
		  }
	  }

	  det = xs[0][0]*xs[1][1] - xs[0][1]*xs[1][0];

	  if (det < 0.0) {
          opserr << "WARNING NineFourNodeQuadUP: Determinant<=0 in tag " 
	    	     << this->getTag();
		  exit(-1);
	  }

      for (i=0; i<nen; i++) {
          if (mode==0) { 
             temp = (shgu[0][i][m]*xs[1][1] - shgu[1][i][m]*xs[0][1])/det;
		     shgu[1][i][m] = (-shgu[0][i][m]*xs[1][0] + shgu[1][i][m]*xs[0][0])/det;
		     shgu[0][i][m] = temp;
          }
          if (mode==1) { 
             temp = (shgp[0][i][m]*xs[1][1] - shgp[1][i][m]*xs[0][1])/det;
		     shgp[1][i][m] = (-shgp[0][i][m]*xs[1][0] + shgp[1][i][m]*xs[0][0])/det;
		     shgp[0][i][m] = temp;
          }
          if (mode==2) { 
             temp = (shgq[0][i][m]*xs[1][1] - shgq[1][i][m]*xs[0][1])/det;
		     shgq[1][i][m] = (-shgq[0][i][m]*xs[1][0] + shgq[1][i][m]*xs[0][0])/det;
		     shgq[0][i][m] = temp;
          }
	  }

	  dvol[m] = w[m]*thickness*det;

  }  //end of m loop

}


void NineFourNodeQuadUP::shapeFunction(double *w, int nint, int nen, int mode)
{
  static const double ra[] = {-0.5,0.5,0.5,-0.5,0.,0.5,0.,-0.5,0.};
  static const double sa[] = {-0.5,-0.5,0.5,0.5,-0.5,0.,0.5,0.,0.};

  double g, r, s, shl19, shl29, shl39, tempr, temps;
  int ic, il, is;

  g = 0.;
  if (nint == 4) {
	  g=2./sqrt(3.0);
	  w[0] = w[1] = w[2] = w[3] = 1.;
  }
  if (nint == 9) {
	  g=2.*sqrt(3.0/5.0);
	  w[0] = w[1] = w[2] = w[3] = 25./81.;
	  w[4] = w[5] = w[6] = w[7] = 40./81.;
	  w[8] = 64./81.;
  }

  for (int i=0; i<nint; i++) {
    r = g*ra[i];
	s = g*sa[i];
	shl19 = shl29 = shl39 = 0.;
	if (nen > 4) {
		tempr = 1.-r*r;
		temps = 1.-s*s;
		if (nen == 9) {
			if (mode==0) {
	  		   shlu[0][8][i] = -2.*r*temps;
			   shl19 = 0.5*shlu[0][8][i];
			   shlu[1][8][i] = -2.*s*tempr;
			   shl29 = 0.5*shlu[1][8][i];
			   shlu[2][8][i] = temps*tempr;
			   shl39 = 0.5*shlu[2][8][i];
			}
			if (mode==2) {
	  		   shlq[0][8][i] = -2.*r*temps;
			   shl19 = 0.5*shlq[0][8][i];
			   shlq[1][8][i] = -2.*s*tempr;
			   shl29 = 0.5*shlq[1][8][i];
			   shlq[2][8][i] = temps*tempr;
			   shl39 = 0.5*shlq[2][8][i];
			}
        }
		if (mode==0) {
		   shlu[0][4][i] = -r*(1.-s) - shl19;
		   shlu[1][4][i] = -0.5*tempr - shl29;
           shlu[2][4][i] = 0.5*tempr*(1.-s) - shl39;
		   shlu[0][5][i] = 0.5*temps - shl19;
		   shlu[1][5][i] = -s*(1.+r) - shl29;
           shlu[2][5][i] = 0.5*temps*(1.+r) - shl39;
		   shlu[0][6][i] = -r*(1.+s) - shl19;
		   shlu[1][6][i] = 0.5*tempr - shl29;
           shlu[2][6][i] = 0.5*tempr*(1.+s) - shl39;
		   shlu[0][7][i] = -0.5*temps - shl19;
		   shlu[1][7][i] = -s*(1.-r) - shl29;
           shlu[2][7][i] = 0.5*temps*(1.-r) - shl39;
		}
		if (mode==2) {
		   shlq[0][4][i] = -r*(1.-s) - shl19;
		   shlq[1][4][i] = -0.5*tempr - shl29;
           shlq[2][4][i] = 0.5*tempr*(1.-s) - shl39;
		   shlq[0][5][i] = 0.5*temps - shl19;
		   shlq[1][5][i] = -s*(1.+r) - shl29;
           shlq[2][5][i] = 0.5*temps*(1.+r) - shl39;
		   shlq[0][6][i] = -r*(1.+s) - shl19;
		   shlq[1][6][i] = 0.5*tempr - shl29;
           shlq[2][6][i] = 0.5*tempr*(1.+s) - shl39;
		   shlq[0][7][i] = -0.5*temps - shl19;
		   shlq[1][7][i] = -s*(1.-r) - shl29;
           shlq[2][7][i] = 0.5*temps*(1.-r) - shl39;
		}
	}

    for (int k=0; k<4; k++) {
        tempr = 0.5 + ra[k]*r;
		temps = 0.5 + sa[k]*s;
		if (mode==0) {
		   shlu[0][k][i] = ra[k]*temps - 0.5*shl19;
		   shlu[1][k][i] = tempr*sa[k] - 0.5*shl29;
           shlu[2][k][i] = tempr*temps - 0.5*shl39;
		}
		if (mode==1) {
		   shlp[0][k][i] = ra[k]*temps - 0.5*shl19;
		   shlp[1][k][i] = tempr*sa[k] - 0.5*shl29;
           shlp[2][k][i] = tempr*temps - 0.5*shl39;
		}
		if (mode==2) {
		   shlq[0][k][i] = ra[k]*temps - 0.5*shl19;
		   shlq[1][k][i] = tempr*sa[k] - 0.5*shl29;
           shlq[2][k][i] = tempr*temps - 0.5*shl39;
		}
	}

    if (nen > 4) {
        for (int m=4; m<8; m++) {
            ic = m - 4;
			il = m - 3;
			is = 1;
			if (m==7) {
                ic = 0;
				il = 3;
				is = 3;
			}
            for (int j=ic; j<=il; j+=is) {
		        if (mode==0) {
                   shlu[0][j][i] = shlu[0][j][i] - 0.5*shlu[0][m][i];
                   shlu[1][j][i] = shlu[1][j][i] - 0.5*shlu[1][m][i];
                   shlu[2][j][i] = shlu[2][j][i] - 0.5*shlu[2][m][i];
				}
		        if (mode==2) {
                   shlq[0][j][i] = shlq[0][j][i] - 0.5*shlq[0][m][i];
                   shlq[1][j][i] = shlq[1][j][i] - 0.5*shlq[1][m][i];
                   shlq[2][j][i] = shlq[2][j][i] - 0.5*shlq[2][m][i];
				}
			}
		}  //end of m loop
	}
  }  //end of i loop
}


double NineFourNodeQuadUP::mixtureRho(int i)
{
  double rhoi, e, n;
	rhoi= theMaterial[i]->getRho();
	e = 0.7;  //theMaterial[i]->getVoidRatio();
  n = e / (1.0 + e);
  //return n * rho + (1.0-n) * rhoi;
	return rhoi;
}

