/*  DYNAMO:- Event driven molecular dynamics simulator 
    http://www.marcusbannerman.co.uk/dynamo
    Copyright (C) 2008  Marcus N Campbell Bannerman <m.bannerman@gmail.com>

    This program is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    version 3 as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "NewtonL.hpp"
#include "../../extcode/xmlwriter.hpp"
#include "../interactions/intEvent.hpp"
#include "../2particleEventData.hpp"
#include "../NparticleEventData.hpp"
#include "../dynamics.hpp"
#include "../BC/BC.hpp"
#include "../../base/is_exception.hpp"
#include "../../base/is_simdata.hpp"
#include "../species/species.hpp"
#include "../../schedulers/sorters/datastruct.hpp"

bool 
CLNewton::CubeCubeInRoot(CPDData& dat, const Iflt& d) const
{
  //To be approaching, the largest dimension of rij must be being
  //reduced
  
  size_t largedim(0);
  for (size_t iDim(1); iDim < NDIM; ++iDim)
    if (fabs(dat.rij[iDim]) > fabs(dat.rij[largedim])) largedim = iDim;
    
  if (dat.rij[largedim] * dat.vij[largedim] < 0)
    {      
      Iflt tInMax(-HUGE_VAL), tOutMin(HUGE_VAL);
      
      for (size_t iDim(0); iDim < NDIM; ++iDim)
	{
	  Iflt tmptime1 = -(dat.rij[iDim] + d) / dat.vij[iDim];
	  Iflt tmptime2 = -(dat.rij[iDim] - d) / dat.vij[iDim];
	  
	  if (tmptime1 < tmptime2)
	    {
	      if (tmptime1 > tInMax) tInMax = tmptime1;
	      if (tmptime2 < tOutMin) tOutMin = tmptime2;
	    }
	  else
	    {
	      if (tmptime2 > tInMax) tInMax = tmptime2;
	      if (tmptime1 < tOutMin) tOutMin = tmptime1;
	    }
	}
      
      if (tInMax < tOutMin)
	{
	  dat.dt = tInMax;
	  return true;
	}
    }
  
  return false;
}

bool 
CLNewton::CubeCubeOutRoot(CPDData& dat, const Iflt& d) const
{
  D_throw() << "Not implemented";
}

bool 
CLNewton::cubeOverlap(const CPDData& dat, const Iflt& d) const
{
  for (size_t iDim(0); iDim < NDIM; ++iDim)
    if (fabs(dat.rij[iDim]) > d) return false;
  
  return true;
}

bool 
CLNewton::SphereSphereInRoot(CPDData& dat, const Iflt& d2) const
{
  if (dat.rvdot < 0)
    {
      Iflt arg = dat.rvdot * dat.rvdot - dat.v2 * (dat.r2 - d2);

      if (arg > 0)
	{
	  //This is the more numerically stable form of the quadratic
	  //formula
	  dat.dt = (d2 - dat.r2) / (dat.rvdot-sqrt(arg));

#ifdef DYNAMO_DEBUG
	  if (std::isnan(dat.dt))
	    D_throw() << "dat.dt is nan";
#endif
	  return true;
	}
      else 
	return false;
    }
  else
    return false;
}
  
bool 
CLNewton::SphereSphereOutRoot(CPDData& dat, const Iflt& d2) const
{
  dat.dt = (sqrt(dat.rvdot * dat.rvdot - dat.v2 * (dat.r2 - d2))-dat.rvdot) / dat.v2;

  if (isnan(dat.dt))
    {//The nan occurs if the spheres aren't moving apart
      dat.dt = HUGE_VAL;
      return false;
    }
  else
    return true;
}

bool 
CLNewton::sphereOverlap(const CPDData& dat, const Iflt& d2) const
{
  return (dat.r2 - d2) < 0.0;
}

C1ParticleData 
CLNewton::randomGaussianEvent(const CParticle& part, const Iflt& sqrtT) const
{
  //See http://mathworld.wolfram.com/SpherePointPicking.html

  //Ensure the particle is free streamed first
  updateParticle(part);

  //Collect the precoll data
  C1ParticleData tmpDat(part, Sim->Dynamics.getSpecies(part), GAUSSIAN);

  Iflt factor = 
    sqrtT / std::sqrt(tmpDat.getSpecies().getMass());

  //Assign the new velocities
  for (int iDim = 0; iDim < NDIM; iDim++)
    const_cast<CParticle&>(part).getVelocity()[iDim] 
      = Sim->normal_sampler() * factor;

  tmpDat.setDeltaKE(0.5 * tmpDat.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - tmpDat.getOldVel().nrm2()));
  
  return tmpDat;
}

CLNewton::CLNewton(DYNAMO::SimData* tmp):
  CLiouvillean(tmp)
{}

void
CLNewton::streamParticle(CParticle &particle, const Iflt &dt) const
{
  for (size_t iDim(0); iDim < NDIM; ++iDim)
    particle.getPosition()[iDim] +=  particle.getVelocity()[iDim] * dt;
}

Iflt 
CLNewton::getWallCollision(const CParticle &part, 
			   const Vector  &wallLoc, 
			   const Vector  &wallNorm) const
{
  Vector  rij = part.getPosition(),
    vel = part.getVelocity();

  Sim->Dynamics.BCs().setPBC(rij, vel);

  Iflt rvdot = (vel | wallNorm);

  rij -= wallLoc;

  if (rvdot < 0)
    return  - ((rij | wallNorm) / rvdot);
  
  return HUGE_VAL;
}


C1ParticleData 
CLNewton::runWallCollision(const CParticle &part, 
			   const Vector  &vNorm,
			   const Iflt& e
			   ) const
{
  updateParticle(part);

  C1ParticleData retVal(part, Sim->Dynamics.getSpecies(part), WALL);
  
  const_cast<CParticle&>(part).getVelocity()
    -= (1+e) * (vNorm | part.getVelocity()) * vNorm;
  
  retVal.setDeltaKE(0.5 * retVal.getSpecies().getMass()
		    * (part.getVelocity().nrm2() 
		       - retVal.getOldVel().nrm2()));
  
  return retVal; 
}

C1ParticleData 
CLNewton::runAndersenWallCollision(const CParticle& part, 
				   const Vector & vNorm,
				   const Iflt& sqrtT
				   ) const
{  
  updateParticle(part);

  //This gives a completely new random unit vector with a properly
  //distributed Normal component. See Granular Simulation Book
  C1ParticleData tmpDat(part, Sim->Dynamics.getSpecies(part), WALL);
 
  for (int iDim = 0; iDim < NDIM; iDim++)
    const_cast<CParticle&>(part).getVelocity()[iDim] 
      = Sim->normal_sampler() * sqrtT 
      / sqrt(Sim->Dynamics.getSpecies(part).getMass());
  
  const_cast<CParticle&>(part).getVelocity() 
    //This first line adds a component in the direction of the normal
    += vNorm * (sqrtT * sqrt(-2.0*log(1.0-Sim->uniform_sampler())
			     / Sim->Dynamics.getSpecies(part).getMass())
		//This removes the original normal component
		-(part.getVelocity() | vNorm));

  tmpDat.setDeltaKE(0.5 * tmpDat.getSpecies().getMass()
		    * (part.getVelocity().nrm2()
		       - tmpDat.getOldVel().nrm2()));
  
  return tmpDat; 
}

Iflt
CLNewton::getSquareCellCollision2(const CParticle& part, 
				 const Vector & origin, 
				 const Vector & width) const
{
  Vector  rpos(part.getPosition() - origin);
  Vector  vel(part.getVelocity());
  Sim->Dynamics.BCs().setPBC(rpos, vel);
  
  Iflt retVal((vel[0] < 0)
	      ? -rpos[0]/vel[0] 
	      : (width[0]-rpos[0]) / vel[0]);

  for (int iDim = 1; iDim < NDIM; ++iDim)
    {
      Iflt tmpdt((vel[iDim] < 0)
		 ? -rpos[iDim]/vel[iDim] 
		 : (width[iDim]-rpos[iDim]) / vel[iDim]);
      
      if (tmpdt < retVal)
	retVal = tmpdt;
    }
  
  return retVal;
}

size_t
CLNewton::getSquareCellCollision3(const CParticle& part, 
				 const Vector & origin, 
				 const Vector & width) const
{
  Vector  rpos(part.getPosition() - origin);
  Vector  vel(part.getVelocity());

  Sim->Dynamics.BCs().setPBC(rpos, vel);

  size_t retVal(0);
  Iflt time((vel[0] < 0) ? -rpos[0]/vel[0] : (width[0]-rpos[0]) / vel[0]);
  

  for (size_t iDim = 1; iDim < NDIM; ++iDim)
    {
      Iflt tmpdt = ((vel[iDim] < 0) 
		  ? -rpos[iDim]/vel[iDim] 
		  : (width[iDim]-rpos[iDim]) / vel[iDim]);

      if (tmpdt < time)
	{
	  time = tmpdt;
	  retVal = iDim;
	}
    }

  return retVal;
}

bool 
CLNewton::DSMCSpheresTest(const CParticle& p1, 
			  const CParticle& p2, 
			  Iflt& maxprob,
			  const Iflt& factor,
			  CPDData& pdat) const
{
  pdat.vij = p1.getVelocity() - p2.getVelocity();

  //Sim->Dynamics.BCs().setPBC(pdat.rij, pdat.vij);
  pdat.rvdot = (pdat.rij | pdat.vij);
  
  if (pdat.rvdot > 0)
    return false; //Positive rvdot

  Iflt prob = factor * (-pdat.rvdot);

  if (prob > maxprob)
    maxprob = prob;

  return prob > Sim->uniform_sampler() * maxprob;
}

C2ParticleData
CLNewton::DSMCSpheresRun(const CParticle& p1, 
			 const CParticle& p2, 
			 const Iflt& e,
			 CPDData& pdat) const
{
  updateParticlePair(p1, p2);  

  C2ParticleData retVal(p1, p2,
			Sim->Dynamics.getSpecies(p1),
			Sim->Dynamics.getSpecies(p2),
			CORE);
  
  retVal.rij = pdat.rij;
  retVal.rvdot = pdat.rvdot;

  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/(p1Mass+p2Mass);

  retVal.dP = retVal.rij * ((1.0 + e) * mu * retVal.rvdot 
			    / retVal.rij.nrm2());  

  //This function must edit particles so it overrides the const!
  const_cast<CParticle&>(p1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<CParticle&>(p2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (p1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (p2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}


C2ParticleData 
CLNewton::SmoothSpheresColl(const CIntEvent& event, const Iflt& e,
			    const Iflt&, const EEventType& eType) const
{
  const CParticle& particle1 = Sim->vParticleList[event.getParticle1ID()];
  const CParticle& particle2 = Sim->vParticleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);  

  C2ParticleData retVal(particle1, particle2,
			Sim->Dynamics.getSpecies(particle1),
			Sim->Dynamics.getSpecies(particle2),
			eType);
    
  Sim->Dynamics.BCs().setPBC(retVal.rij, retVal.vijold);
  
  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/(p1Mass+p2Mass);
  
  retVal.rvdot = (retVal.rij | retVal.vijold);
  retVal.dP = retVal.rij * ((1.0 + e) * mu * retVal.rvdot / retVal.rij.nrm2());  

  //This function must edit particles so it overrides the const!
  const_cast<CParticle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<CParticle&>(particle2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

C2ParticleData 
CLNewton::parallelCubeColl(const CIntEvent& event, const Iflt& e,
			   const Iflt&, const EEventType& eType) const
{
  const CParticle& particle1 = Sim->vParticleList[event.getParticle1ID()];
  const CParticle& particle2 = Sim->vParticleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);

  C2ParticleData retVal(particle1, particle2,
			Sim->Dynamics.getSpecies(particle1),
			Sim->Dynamics.getSpecies(particle2),
			eType);
    
  Sim->Dynamics.BCs().setPBC(retVal.rij, retVal.vijold);
  
  size_t dim(0);
   
  for (size_t iDim(1); iDim < NDIM; ++iDim)
    if (fabs(retVal.rij[dim]) < fabs(retVal.rij[iDim])) dim = iDim;

  Iflt p1Mass = retVal.particle1_.getSpecies().getMass(); 
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass/(p1Mass+p2Mass);
  
  Vector collvec(0,0,0);

  if (retVal.rij[dim] < 0)
    collvec[dim] = -1;
  else
    collvec[dim] = 1;

  retVal.rvdot = (retVal.rij | retVal.vijold);

  retVal.dP = collvec * (1.0 + e) * mu * (collvec | retVal.vijold);  

  //This function must edit particles so it overrides the const!
  const_cast<CParticle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<CParticle&>(particle2).getVelocity() += retVal.dP / p2Mass;

  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

CNParticleData 
CLNewton::multibdyCollision(const CRange& range1, const CRange& range2, 
			    const Iflt&, const EEventType& eType) const
{
  Vector  COMVel1(0,0,0), COMVel2(0,0,0), COMPos1(0,0,0), COMPos2(0,0,0);
  
  Iflt structmass1(0), structmass2(0);
  
  BOOST_FOREACH(const size_t& ID, range1)
    {
      updateParticle(Sim->vParticleList[ID]);
      
      structmass1 += 
	Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMVel1 += Sim->vParticleList[ID].getVelocity()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMPos1 += Sim->vParticleList[ID].getPosition()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
    }
  
  BOOST_FOREACH(const size_t& ID, range2)
    {
      updateParticle(Sim->vParticleList[ID]);

      structmass2 += 
	Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMVel2 += Sim->vParticleList[ID].getVelocity()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMPos2 += Sim->vParticleList[ID].getPosition()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
    }
  
  COMVel1 /= structmass1;
  COMVel2 /= structmass2;
  
  COMPos1 /= structmass1;
  COMPos2 /= structmass2;
  
  Vector  rij = COMPos1 - COMPos2, vij = COMVel1 - COMVel2;
  Sim->Dynamics.BCs().setPBC(rij, vij);
  Iflt rvdot = (rij | vij);

  Iflt mu = structmass1 * structmass2 / (structmass1 + structmass2);

  static const Iflt e = 1.0;
  Vector  dP = rij * ((1.0 + e) * mu * rvdot / rij.nrm2());

  CNParticleData retVal;
  BOOST_FOREACH(const size_t& ID, range1)
    {
      C1ParticleData tmpval
	(Sim->vParticleList[ID],
	 Sim->Dynamics.getSpecies(Sim->vParticleList[ID]),
	 eType);

      const_cast<CParticle&>(tmpval.getParticle()).getVelocity()
	-= dP / structmass1;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
      
      retVal.L1partChanges.push_back(tmpval);
    }

  BOOST_FOREACH(const size_t& ID, range2)
    {
      C1ParticleData tmpval
	(Sim->vParticleList[ID],
	 Sim->Dynamics.getSpecies(Sim->vParticleList[ID]),
	 eType);

      const_cast<CParticle&>(tmpval.getParticle()).getVelocity()
	+= dP / structmass2;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
  
      retVal.L1partChanges.push_back(tmpval);
    }
  
  return retVal;
}

CNParticleData 
CLNewton::multibdyWellEvent(const CRange& range1, const CRange& range2, 
			    const Iflt&, const Iflt& deltaKE, EEventType& eType) const
{
  Vector  COMVel1(0,0,0), COMVel2(0,0,0), COMPos1(0,0,0), COMPos2(0,0,0);
  
  Iflt structmass1(0), structmass2(0);
  
  BOOST_FOREACH(const size_t& ID, range1)
    {
      updateParticle(Sim->vParticleList[ID]);
      
      structmass1 += 
	Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMVel1 += Sim->vParticleList[ID].getVelocity()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMPos1 += Sim->vParticleList[ID].getPosition()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
    }
  
  BOOST_FOREACH(const size_t& ID, range2)
    {
      updateParticle(Sim->vParticleList[ID]);

      structmass2 += 
	Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMVel2 += Sim->vParticleList[ID].getVelocity()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
      
      COMPos2 += Sim->vParticleList[ID].getPosition()
	* Sim->Dynamics.getSpecies(Sim->vParticleList[ID]).getMass();
    }
  
  COMVel1 /= structmass1;
  COMVel2 /= structmass2;
  
  COMPos1 /= structmass1;
  COMPos2 /= structmass2;
  
  Vector  rij = COMPos1 - COMPos2, vij = COMVel1 - COMVel2;
  Sim->Dynamics.BCs().setPBC(rij, vij);
  Iflt rvdot = (rij | vij);

  Iflt mu = structmass1 * structmass2 / (structmass1 + structmass2);

  Iflt R2 = rij.nrm2();
  Iflt sqrtArg = rvdot * rvdot + 2.0 * R2 * deltaKE / mu;

  Vector  dP(0,0,0);

  if ((deltaKE < 0) && (sqrtArg < 0))
    {
      eType = BOUNCE;
      dP = rij * 2.0 * mu * rvdot / R2;
    }
  else
    {
      if (deltaKE < 0)
	eType = WELL_KEDOWN;
      else
	eType = WELL_KEUP;
	  
      if (rvdot < 0)
	dP = rij 
	  * (2.0 * deltaKE / (std::sqrt(sqrtArg) - rvdot));
      else
	dP = rij 
	  * (-2.0 * deltaKE / (rvdot + std::sqrt(sqrtArg)));
    }
  
  CNParticleData retVal;
  BOOST_FOREACH(const size_t& ID, range1)
    {
      C1ParticleData tmpval
	(Sim->vParticleList[ID],
	 Sim->Dynamics.getSpecies(Sim->vParticleList[ID]),
	 eType);

      const_cast<CParticle&>(tmpval.getParticle()).getVelocity()
	-= dP / structmass1;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
        
      retVal.L1partChanges.push_back(tmpval);
    }

  BOOST_FOREACH(const size_t& ID, range2)
    {
      C1ParticleData tmpval
	(Sim->vParticleList[ID],
	 Sim->Dynamics.getSpecies(Sim->vParticleList[ID]),
	 eType);

      const_cast<CParticle&>(tmpval.getParticle()).getVelocity()
	+= dP / structmass2;
      
      tmpval.setDeltaKE(0.5 * tmpval.getSpecies().getMass()
			* (tmpval.getParticle().getVelocity().nrm2() 
			   - tmpval.getOldVel().nrm2()));
      
      retVal.L1partChanges.push_back(tmpval);
    }
  
  return retVal;
}

C2ParticleData 
CLNewton::SphereWellEvent(const CIntEvent& event, const Iflt& deltaKE, 
			  const Iflt &) const
{
  const CParticle& particle1 = Sim->vParticleList[event.getParticle1ID()];
  const CParticle& particle2 = Sim->vParticleList[event.getParticle2ID()];

  updateParticlePair(particle1, particle2);  

  C2ParticleData retVal(particle1, particle2,
			Sim->Dynamics.getSpecies(particle1),
			Sim->Dynamics.getSpecies(particle2),
			event.getType());
    
  Sim->Dynamics.BCs().setPBC(retVal.rij,retVal.vijold);
  
  retVal.rvdot = (retVal.rij | retVal.vijold);
  
  Iflt p1Mass = retVal.particle1_.getSpecies().getMass();
  Iflt p2Mass = retVal.particle2_.getSpecies().getMass();
  Iflt mu = p1Mass * p2Mass / (p1Mass + p2Mass);  
  Iflt R2 = retVal.rij.nrm2();
  Iflt sqrtArg = retVal.rvdot * retVal.rvdot + 2.0 * R2 * deltaKE / mu;
  
  if ((deltaKE < 0) && (sqrtArg < 0))
    {
      event.setType(BOUNCE);
      retVal.setType(BOUNCE);
      retVal.dP = retVal.rij * 2.0 * mu * retVal.rvdot / R2;
    }
  else
    {
      if (deltaKE < 0)
	{
	  event.setType(WELL_KEDOWN);
	  retVal.setType(WELL_KEDOWN);
	}
      else
	{
	  event.setType(WELL_KEUP);
	  retVal.setType(WELL_KEUP);	  
	}
	  
      retVal.particle1_.setDeltaU(-0.5 * deltaKE);
      retVal.particle2_.setDeltaU(-0.5 * deltaKE);	  
      
      if (retVal.rvdot < 0)
	retVal.dP = retVal.rij 
	  * (2.0 * deltaKE / (std::sqrt(sqrtArg) - retVal.rvdot));
      else
	retVal.dP = retVal.rij 
	  * (-2.0 * deltaKE / (retVal.rvdot + std::sqrt(sqrtArg)));
    }
  
#ifdef DYNAMO_DEBUG
  if (isnan(retVal.dP[0]))
    D_throw() << "A nan dp has ocurred";
#endif
  
  //This function must edit particles so it overrides the const!
  const_cast<CParticle&>(particle1).getVelocity() -= retVal.dP / p1Mass;
  const_cast<CParticle&>(particle2).getVelocity() += retVal.dP / p2Mass;
  
  retVal.particle1_.setDeltaKE(0.5 * retVal.particle1_.getSpecies().getMass()
			       * (particle1.getVelocity().nrm2() 
				  - retVal.particle1_.getOldVel().nrm2()));
  
  retVal.particle2_.setDeltaKE(0.5 * retVal.particle2_.getSpecies().getMass()
			       * (particle2.getVelocity().nrm2() 
				  - retVal.particle2_.getOldVel().nrm2()));

  return retVal;
}

void 
CLNewton::outputXML(xmlw::XmlStream& XML) const
{
  XML << xmlw::attr("Type") 
      << "Newtonian";
}

Iflt 
CLNewton::getPBCSentinelTime(const CParticle& part, const Iflt& lMax) const
{
#ifdef DYNAMO_DEBUG
  if (!isUpToDate(part))
    D_throw() << "Particle is not up to date";
#endif

  Vector pos(part.getPosition()), vel(part.getVelocity());

  Sim->Dynamics.BCs().setPBC(pos, vel);

  Iflt retval = (0.5 * Sim->aspectRatio[0] - lMax) / fabs(vel[0]);

  for (size_t i(1); i < NDIM; ++i)
    {
      Iflt tmp = (0.5 * Sim->aspectRatio[i] - lMax) / fabs(vel[i]);

      if (tmp < retval)
	retval = tmp;
    }

  return retval;
}
