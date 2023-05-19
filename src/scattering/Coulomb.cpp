
#include "Coulomb.H"
#include "MathUtils.H"
#include "PicSpecies.H"
#include "JustinsParticle.H"
#include "JustinsParticlePtr.H"
#include "ParticleData.H"
#include "BinFab.H"
#include "ScatteringUtils.H"

#include "NamespaceHeader.H"


void Coulomb::initialize( const PicSpeciesPtrVect&  a_picSpeciesPtrVect,
                          const DomainGrid&         a_mesh )
{
   CH_TIME("Coulomb::initialize()");
   
   // get pointer to species 1 and assert collisions allowed
   CH_assert(m_sp1<a_picSpeciesPtrVect.size());
   PicSpeciesPtr this_species1(a_picSpeciesPtrVect[m_sp1]);
   //CH_assert(this_sSpecies1->scatter());
      
   // get pointer to species 2 and assert collisions allowed
   CH_assert(m_sp2<a_picSpeciesPtrVect.size());
   PicSpeciesPtr this_species2(a_picSpeciesPtrVect[m_sp2]);
   //CH_assert(this_species2->scatter());

   // set the species names
   m_species1_name = this_species1->name();
   m_species2_name = this_species2->name();
   
   // set the species charges and assert they are not neutrals
   m_charge1 = this_species1->charge();  // species 1 charge / |qe|
   m_charge2 = this_species2->charge();  // species 2 charge / |qe|
   CH_assert(m_charge1!=0);
   CH_assert(m_charge2!=0);

   // set the species masses
   m_mass1 = this_species1->mass();    // species 1 mass / me
   m_mass2 = this_species2->mass();    // species 2 mass / me
   m_mu = m_mass1*m_mass2/(m_mass1 + m_mass2); // reduced mass
  
   Real cvacSq = Constants::CVAC*Constants::CVAC;
#ifdef RELATIVISTIC_PARTICLES
   m_b90_fact = abs(m_charge1*m_charge2)/cvacSq*m_b90_codeToPhys; 
#else
   m_b90_fact = abs(m_charge1*m_charge2)/(m_mu*cvacSq)*m_b90_codeToPhys/2.0; 
#endif

   // set the mean free time
   if(this_species1->scatter() && this_species2->scatter()) {
   
      const bool setMoments = true;
      const LevelData<FArrayBox>& numberDensity1 = this_species1->getNumberDensity(setMoments);
      const LevelData<FArrayBox>& energyDensity1 = this_species1->getEnergyDensity(setMoments);
   
      if(m_sp1==m_sp2) {
         setMeanFreeTime(numberDensity1,energyDensity1);
      }
      else {
         const LevelData<FArrayBox>& numberDensity2 = this_species2->getNumberDensity(setMoments);
         const LevelData<FArrayBox>& energyDensity2 = this_species2->getEnergyDensity(setMoments);
         setMeanFreeTime(numberDensity1,energyDensity1,numberDensity2,energyDensity2);
      }

   }

   if (m_verbosity)  printParameters();

}

void Coulomb::setMeanFreeTime( const LevelData<FArrayBox>&  a_numberDensity,
                               const LevelData<FArrayBox>&  a_energyDensity ) const
{
   CH_TIME("Coulomb::setMeanFreeTime()");
 
   Real Teff_eV, numberDensity, energyDensity;
   Real tau;
   Real box_nuMax=0.0; // for scattering time step calculation
 
   // nu_12 = q1^2*q2^2*n2*Clog/(8*pi*ep0^2*mu^2*Vab^3)
   //Real nu0 = pow(Constants::QE,4.0)/8.0/Constants::PI/pow(Constants::EP0,2.0);
   Real cvacSq = Constants::CVAC*Constants::CVAC;
    
   const DisjointBoxLayout& grids = a_numberDensity.disjointBoxLayout();
   DataIterator ditg(grids);
   for (ditg.begin(); ditg.ok(); ++ditg) { // loop over boxes

      const FArrayBox& this_numberDensity = a_numberDensity[ditg];
      const FArrayBox& this_energyDensity = a_energyDensity[ditg];
     
      const Box gridBox = grids.get(ditg);
      BoxIterator gbit(gridBox);
      for (gbit.begin(); gbit.ok(); ++gbit) { // loop over cells
         const IntVect ig = gbit(); // grid index
       
         // get local density and temperature and compute local VTeff
         numberDensity = this_numberDensity.get(ig,0); // [1/m^3]
         if(numberDensity == 0.0) continue;
         
         energyDensity = 0.0;
         for( int dir=0; dir<3; dir++) {
            energyDensity = energyDensity + this_energyDensity.get(ig,dir);  
         }
         Teff_eV = Constants::ME*2.0/3.0*energyDensity/numberDensity*cvacSq; // [Joules]
         Teff_eV = Constants::EV_PER_JOULE*Teff_eV; // local temperature [eV]

         // define self-species collision time
         
         //Vab = 4.1938e5*sqrt(2.0*Teff_eV/m_mass1); // [m/s]
         //nuab = nu0*pow(m_charge1*m_charge2,2)*m_Clog*numberDensity/(m_mu*m_mu*Vab*Vab*Vab); // [Hz]
         //box_nuMax = Max(box_nuMax,nuab);

         tau = 3.44e5*pow(Teff_eV,1.5)/(numberDensity*Constants::M3_PER_CM3)/m_Clog; // [s]
         tau = tau*sqrt(m_mass1/2.0)/pow(m_charge1*m_charge2,2); // [s]
         box_nuMax = Max(box_nuMax,1.0/tau);
      }
   
   }
   
   Real global_nuMax = box_nuMax;
//#ifdef CH_MPI
//   MPI_Allreduce( &box_nuMax, &global_nuMax, 1, MPI_CH_REAL, MPI_MAX, MPI_COMM_WORLD ); 
//#endif
   m_scatter_dt = 1.0/global_nuMax; // mean free time [s]
   
}

void Coulomb::setMeanFreeTime( const LevelData<FArrayBox>&  a_numberDensity1,
                               const LevelData<FArrayBox>&  a_energyDensity1,
                               const LevelData<FArrayBox>&  a_numberDensity2,
                               const LevelData<FArrayBox>&  a_energyDensity2 ) const
{
   CH_TIME("Coulomb::setMeanFreeTime()");
 
   //std::array<Real,8> x0 = {0.1,0.5,1.0,3.0,6.0,9.0,10.0,100};
   //for (auto i=0; i<x0.size(); i++) {
   //   if(!procID()) cout << "JRA: x0 = " << x0[i] << endl; 
   //   if(!procID()) cout << "JRA: gammainc(x0,1.5) = " << MathUtils::gammainc(x0[i],1.5) << endl; 
   //}
  
   Real Teff1_eV, numberDensity1, energyDensity1, energy1, VT1;
   Real Teff2_eV, numberDensity2, energyDensity2, energy2, VT2;
   Real x12, psi12, nu012, nu12;
   Real x21, psi21, nu021, nu21;
         
   const Real factor = pow(Constants::QE*m_charge1*Constants::QE*m_charge2/Constants::EP0,2)/Constants::FOURPI; // [Joules-m]

   Real box_nuMax=0.0; // for scattering time step calculation
   
   Real cvacSq = Constants::CVAC*Constants::CVAC;
 
   //bool verbosity = true;
   bool verbosity = false;

   const DisjointBoxLayout& grids = a_numberDensity1.disjointBoxLayout();
   DataIterator ditg(grids);
   for (ditg.begin(); ditg.ok(); ++ditg) { // loop over boxes

      const FArrayBox& this_numberDensity1 = a_numberDensity1[ditg];
      const FArrayBox& this_energyDensity1 = a_energyDensity1[ditg];
      const FArrayBox& this_numberDensity2 = a_numberDensity2[ditg];
      const FArrayBox& this_energyDensity2 = a_energyDensity2[ditg];
     
      const Box gridBox = grids.get(ditg);
      BoxIterator gbit(gridBox);
      for (gbit.begin(); gbit.ok(); ++gbit) { // loop over cells
         const IntVect ig = gbit(); // grid index
       
         // get local density and temperature and compute local VTeff
         numberDensity1 = this_numberDensity1.get(ig,0); // [1/m^3]
         numberDensity2 = this_numberDensity2.get(ig,0); // [1/m^3]
         if(numberDensity1*numberDensity2 == 0.0) continue;
         
         energyDensity1 = 0.0;
         energyDensity2 = 0.0;
         for( int dir=0; dir<3; dir++) {
            energyDensity1 = energyDensity1 + this_energyDensity1.get(ig,dir);  
            energyDensity2 = energyDensity2 + this_energyDensity2.get(ig,dir);  
         }
         energy1 = Constants::ME*energyDensity1/numberDensity1*cvacSq; // [Joules]
         energy2 = Constants::ME*energyDensity2/numberDensity2*cvacSq; // [Joules]
         Teff1_eV = Constants::EV_PER_JOULE*2.0/3.0*energy1; // local temperature [eV]
         Teff2_eV = Constants::EV_PER_JOULE*2.0/3.0*energy2; // local temperature [eV]
         VT1 = sqrt(Constants::QE*Teff1_eV/(Constants::ME*m_mass1)); // [m/s]
         VT2 = sqrt(Constants::QE*Teff2_eV/(Constants::ME*m_mass2)); // [m/s]

         x12 = (Teff1_eV/m_mass1)/(Teff2_eV/m_mass2); // ratio of velocities squared
         x21 = 1./x12;

         psi12 = 2.0/sqrt(Constants::PI)*MathUtils::gammainc(x12,1.5);
         psi21 = 2.0/sqrt(Constants::PI)*MathUtils::gammainc(x21,1.5);

         nu012 = factor*m_Clog*numberDensity2/(pow(energy1,2))*VT1; // [Hz]
         nu021 = factor*m_Clog*numberDensity1/(pow(energy2,2))*VT2; // [Hz]
         
         nu12 = (1.0 + m_mass1/m_mass2)*psi12*nu012;
         nu21 = (1.0 + m_mass2/m_mass1)*psi21*nu021;

         if(!procID() && verbosity) {
            cout << "JRA: mass1 = " << m_mass1<< endl;
            cout << "JRA: mass2 = " << m_mass2<< endl;
            cout << "JRA: Teff1_eV = " << Teff1_eV << endl;
            cout << "JRA: Teff2_eV = " << Teff2_eV << endl;
            cout << "JRA: VT1 = " << VT1<< endl;
            cout << "JRA: VT2 = " << VT2<< endl;
            cout << "JRA: x12 = " << x12<< endl;
            cout << "JRA: x21 = " << x21<< endl;
            cout << "JRA: numberDen1 = " << numberDensity1 << endl;
            cout << "JRA: numberDen2 = " << numberDensity2 << endl;
            cout << "JRA: psi12 = " << psi12 << endl;
            cout << "JRA: psi21 = " << psi21 << endl;
            cout << "JRA: nu12 = " << nu12 << endl;
            cout << "JRA: nu21 = " << nu21 << endl;
         }

         // compute nuMax [Hz]
         box_nuMax = Max(box_nuMax,nu12);
         box_nuMax = Max(box_nuMax,nu21);

      }
   
   }
   
   Real global_nuMax = box_nuMax;
//#ifdef CH_MPI
//   MPI_Allreduce( &box_nuMax, &global_nuMax, 1, MPI_CH_REAL, MPI_MAX, MPI_COMM_WORLD ); 
//#endif
   m_scatter_dt = 1.0/global_nuMax; // mean free time [s]

}

void Coulomb::applyScattering( PicSpeciesPtrVect&  a_pic_species_ptr_vect,
                         const DomainGrid&         a_mesh,
                         const Real                a_dt_sec ) const
{
   CH_TIME("Coulomb::applyScattering()");
      
   PicSpeciesPtr this_species1(a_pic_species_ptr_vect[m_sp1]);
   if(!this_species1->scatter()) return;

   if(m_sp1==m_sp2) {
      applySelfScattering( *this_species1, a_mesh, a_dt_sec );
   }
   else {
      PicSpeciesPtr this_species2(a_pic_species_ptr_vect[m_sp2]);
      if(!this_species2->scatter()) return;
      applyInterScattering( *this_species1, *this_species2, a_mesh, a_dt_sec );
   }

}
      
void Coulomb::applySelfScattering( PicSpecies&  a_picSpecies, 
                             const DomainGrid&  a_mesh,
                             const Real         a_dt_sec ) const
{
   CH_TIME("Coulomb::applySelfScattering()");
 
   // predefine some pointers to be used below
   JustinsParticle* part1_ptr = NULL;  
   JustinsParticle* part2_ptr = NULL;  
 
   // define references to a_picSpecies
   LevelData<BinFab<JustinsParticlePtr>>& data_binfab_ptr = a_picSpecies.partData_binfab();
   const bool setMoments = false; // It is the job of the caller to make sure the moments are pre-computed
   const LevelData<FArrayBox>& numberDensity = a_picSpecies.getNumberDensity(setMoments);
   
   const LevelData<FArrayBox>& Jacobian = a_mesh.getJcc();
   const double cellV = a_mesh.getMappedCellVolume()*m_volume_scale;
   // cellV*Jacobian = cell volume in SI units

   // predefine some variables
   bool scatter1, scatter2;
   int numCell, Naa;
   Real numDen, den12, cellV_SI, wpMax;
 
   // loop over lists in each cell and test shuffle
   const DisjointBoxLayout& grids = data_binfab_ptr.disjointBoxLayout();
   DataIterator ditg(grids);
   int verbosity=0; // using this as a verbosity flag
   for (ditg.begin(); ditg.ok(); ++ditg) { // loop over boxes

      const FArrayBox& this_numberDensity = numberDensity[ditg];
     
      BinFab<JustinsParticlePtr>& thisBinFab_ptr = data_binfab_ptr[ditg];
   
      std::vector<JustinsParticlePtr> vector_part_ptrs;
      const Box gridBox = grids.get(ditg);
      BoxIterator gbit(gridBox);
      for (gbit.begin(); gbit.ok(); ++gbit) { // loop over cells
         
         const IntVect ig = gbit(); // grid index
         cellV_SI = cellV*Jacobian[ditg].get(ig,0);
       
         // get local density and temperature and compute local tau
         numDen = this_numberDensity.get(ig,0);
         if(numDen == 0.0) continue;

         // get the number of particles in this cell
         List<JustinsParticlePtr>& cell_pList = thisBinFab_ptr(ig,0);
         numCell = cell_pList.length();
         if(numCell < 2) continue;
          
         // set the start index for the loop over particle collisions
         int pstart = 3;        
         if(numCell % 2 == 0) pstart = 0;
   
         Naa = numCell - 1;
    
         // copy the iterators to a vector in order to shuffle
         vector_part_ptrs.clear();
         vector_part_ptrs.reserve(numCell);
         ListIterator<JustinsParticlePtr> lit(cell_pList);
         for (lit.begin(); lit.ok(); ++lit) vector_part_ptrs.push_back(lit());
         std::shuffle(vector_part_ptrs.begin(),vector_part_ptrs.end(),global_rand_gen); 
         
         for (auto p=pstart; p<vector_part_ptrs.size(); p++) { // loop over particle scattering pairs
 
            // get particle data for first particle    
            JustinsParticlePtr& part1 = vector_part_ptrs[p];
            part1_ptr = part1.getPointer();
            std::array<Real,3>& betap1 = part1_ptr->velocity();
            const float wp1 = part1_ptr->weight();
            
            // get particle data for second particle    
            p++; // advance the loop index by 1
            JustinsParticlePtr& part2 = vector_part_ptrs[p];
            part2_ptr = part2.getPointer();
            std::array<Real,3>& betap2 = part2_ptr->velocity();
            const float wp2 = part2_ptr->weight();
            
            wpMax = std::max(wp1,wp2);
            scatter1 = true;
            scatter2 = true;
            if(wp1<wpMax) {if(MathUtils::rand()>wp1/wpMax) scatter2 = false;}
            else if(wp2<wpMax) {if(MathUtils::rand()>wp2/wpMax) scatter1 = false;} 
	    den12 = wpMax*Naa/cellV_SI;

#ifdef RELATIVISTIC_PARTICLES   
            if(wp2<wpMax) {
               LorentzScatter( betap2, betap1, scatter1, m_mass2, m_mass1, den12, a_dt_sec );
            }
            else {
               LorentzScatter( betap1, betap2, scatter2, m_mass1, m_mass2, den12, a_dt_sec );
            }
#else
            // compute deltaU
            std::array<Real,3> deltaU;
	    GalileanScatter( deltaU, betap1, betap2,
                             den12, m_Clog, a_dt_sec );

            // update particle velocities
            if(scatter1) {for (int n=0; n<3; n++) betap1[n] += 0.5*deltaU[n];}
            if(scatter2) {for (int n=0; n<3; n++) betap2[n] -= 0.5*deltaU[n];}
#endif
         }
         if(pstart==3) {
            int p1, p2;
            for (int p=0; p<pstart; p++) { // scatter particles 0,1, and 2
               p1 = p % 2;
               if(p==0) p2 = 1;
               if(p==1) p2 = 2;
               if(p==2) p2 = 2;
  
               // get particle data for first particle    
               JustinsParticlePtr& part1 = vector_part_ptrs[p1];
               part1_ptr = part1.getPointer();
               std::array<Real,3>& betap1 = part1_ptr->velocity();
               const float wp1 = part1_ptr->weight();
            
               // get particle data for second particle    
               JustinsParticlePtr& part2 = vector_part_ptrs[p2];
               part2_ptr = part2.getPointer();
               std::array<Real,3>& betap2 = part2_ptr->velocity();
               const float wp2 = part2_ptr->weight();
            
               wpMax = std::max(wp1,wp2);
               scatter1 = true;
               scatter2 = true;
               if(wp1<wpMax) {if(MathUtils::rand()>wp1/wpMax) scatter2 = false;} 
               else if(wp2<wpMax) {if(MathUtils::rand()>wp2/wpMax) scatter1 = false;} 
               den12 = wpMax*Naa/cellV_SI/2.0;

#ifdef RELATIVISTIC_PARTICLES   
               if(wp2<wpMax) {
                  LorentzScatter( betap2, betap1, scatter1, m_mass2, m_mass1, den12, a_dt_sec );
               }
               else {
                  LorentzScatter( betap1, betap2, scatter2, m_mass1, m_mass2, den12, a_dt_sec );
               }
#else
               // compute deltaU
               std::array<Real,3> deltaU;
               GalileanScatter( deltaU, betap1, betap2,
                                den12, m_Clog, a_dt_sec );     
            
               // update particle velocities
               if(scatter1) {for (int n=0; n<3; n++) betap1[n] += 0.5*deltaU[n];}
               if(scatter2) {for (int n=0; n<3; n++) betap2[n] -= 0.5*deltaU[n];}
#endif
            }
         }
         verbosity=0;

      } // end loop over cells

   } // end loop over boxes
  
 
   // don't forget to set pointers back to NULL and delete
   part1_ptr = NULL;
   part2_ptr = NULL;
   delete part1_ptr;
   delete part2_ptr;
   
   // While we are here, update the mean free time (assuming energyDensity1 has not changed much)
   //setMeanFreeTime(numberDensity,energyDensity);

}

void Coulomb::applyInterScattering( PicSpecies&  a_picSpecies1,
                                    PicSpecies&  a_picSpecies2, 
                              const DomainGrid&  a_mesh,
                              const Real         a_dt_sec ) const
{
   CH_TIME("Coulomb::applyInterScattering()");
   
   CH_assert(m_species1_name==a_picSpecies1.name());
   CH_assert(m_species2_name==a_picSpecies2.name());

   // predefine some pointers to be used below
   JustinsParticle* part1_ptr = NULL;  
   JustinsParticle* part2_ptr = NULL;  
   
   // define references to a_picSpecies1
   LevelData<BinFab<JustinsParticlePtr>>& data_binfab1_ptr = a_picSpecies1.partData_binfab();
   const bool setMoments = false; // It is the job of the caller to make sure the moments are pre-computed
   const LevelData<FArrayBox>& numberDensity1 = a_picSpecies1.getNumberDensity(setMoments);
   
   // define references to a_picSpecies2
   LevelData<BinFab<JustinsParticlePtr>>& data_binfab2_ptr = a_picSpecies2.partData_binfab();
   const LevelData<FArrayBox>& numberDensity2 = a_picSpecies2.getNumberDensity(setMoments);
  
   const LevelData<FArrayBox>& Jacobian = a_mesh.getJcc();
   const double cellV = a_mesh.getMappedCellVolume()*m_volume_scale;
   // cellV*Jacobian = cell volume in SI units
 
   // predefine some variables
   bool scatter1, scatter2;
   int numCell1, numCell2, pMax, pMin;
   Real numDen1, wpMax;
   Real numDen2, den12, cellV_SI;
 
   // loop over lists in each cell and test shuffle
   const DisjointBoxLayout& grids = data_binfab1_ptr.disjointBoxLayout();
   DataIterator ditg(grids);
   int verbosity=0; // using this as a verbosity flag
   for (ditg.begin(); ditg.ok(); ++ditg) { // loop over boxes

      const FArrayBox& this_numberDensity1 = numberDensity1[ditg];
      const FArrayBox& this_numberDensity2 = numberDensity2[ditg];
     
      BinFab<JustinsParticlePtr>& thisBinFab1_ptr = data_binfab1_ptr[ditg];
      BinFab<JustinsParticlePtr>& thisBinFab2_ptr = data_binfab2_ptr[ditg];
   
      std::vector<JustinsParticlePtr> vector_part1_ptrs;
      std::vector<JustinsParticlePtr> vector_part2_ptrs;
      const Box gridBox = grids.get(ditg);
      BoxIterator gbit(gridBox);
      for (gbit.begin(); gbit.ok(); ++gbit) { // loop over cells
         
         const IntVect ig = gbit(); // grid index
         cellV_SI = cellV*Jacobian[ditg].get(ig,0);
         
         // get local density and temperature and compute local tau
         numDen1 = this_numberDensity1.get(ig,0);
         numDen2 = this_numberDensity2.get(ig,0);
         if(numDen1*numDen2 == 0.0) continue;

         // get the number of particles in this cell
         List<JustinsParticlePtr>& cell_pList1 = thisBinFab1_ptr(ig,0);
         List<JustinsParticlePtr>& cell_pList2 = thisBinFab2_ptr(ig,0);
         numCell1 = cell_pList1.length();
         numCell2 = cell_pList2.length();
         if(numCell1*numCell2 < 2) continue;
         pMin = std::min(numCell1,numCell2);
         pMax = std::max(numCell1,numCell2);

         // copy the iterators to a vector in order to shuffle
         vector_part1_ptrs.clear();
         vector_part1_ptrs.reserve(numCell1);
         ListIterator<JustinsParticlePtr> lit1(cell_pList1);
         for (lit1.begin(); lit1.ok(); ++lit1) vector_part1_ptrs.push_back(lit1());
         std::shuffle(vector_part1_ptrs.begin(),vector_part1_ptrs.end(),global_rand_gen); 
         
         // do the same for species 2
         vector_part2_ptrs.clear();
         vector_part2_ptrs.reserve(numCell2);
         ListIterator<JustinsParticlePtr> lit2(cell_pList2);
         for (lit2.begin(); lit2.ok(); ++lit2) vector_part2_ptrs.push_back(lit2());
         std::shuffle(vector_part2_ptrs.begin(),vector_part2_ptrs.end(),global_rand_gen); 
         
         unsigned int p1, p2;
         for (auto p=0; p<pMax; p++) { // loop over particle scattering pairs
     
            if(pMin==numCell1) {
               p1 = p % numCell1;
               p2 = p;
            } 
            else {
               p1 = p;
               p2 = p % numCell2;
            } 

            // get particle data for first particle    
            JustinsParticlePtr& part1 = vector_part1_ptrs[p1];
            part1_ptr = part1.getPointer();
            std::array<Real,3>& betap1 = part1_ptr->velocity();
            const float wp1 = part1_ptr->weight();
            
            // get particle data for second particle    
            JustinsParticlePtr& part2 = vector_part2_ptrs[p2];
            part2_ptr = part2.getPointer();
            std::array<Real,3>& betap2 = part2_ptr->velocity();
            const float wp2 = part2_ptr->weight();

            // determine maximum weight of this pair and set scatter flags (rejection method)
            wpMax = std::max(wp1,wp2);
            scatter1 = true;
            scatter2 = true;
            if(wp1<wpMax) {if(MathUtils::rand()>wp1/wpMax) scatter2 = false;} 
            else if(wp2<wpMax) {if(MathUtils::rand()>wp2/wpMax) scatter1 = false;} 
            den12 = wpMax*pMin/cellV_SI;

#ifdef RELATIVISTIC_PARTICLES
            if(wp2<wpMax) {
               LorentzScatter( betap2, betap1, scatter1, m_mass2, m_mass1, den12, a_dt_sec );
            }
            else {
               LorentzScatter( betap1, betap2, scatter2, m_mass1, m_mass2, den12, a_dt_sec );
            }
#else
            // compute deltaU
            std::array<Real,3> deltaU;
            GalileanScatter( deltaU, betap1, betap2,
                             den12, m_Clog, a_dt_sec );     

            // update particle velocities
            if(scatter1) {for (int n=0; n<3; n++) betap1[n] += m_mu/m_mass1*deltaU[n];}
            if(scatter2) {for (int n=0; n<3; n++) betap2[n] -= m_mu/m_mass2*deltaU[n];}
#endif
         }
         verbosity=0;

      } // end loop over cells

   } // end loop over boxes

   // don't forget to set pointers back to NULL and delete
   part1_ptr = NULL;
   part2_ptr = NULL;
   delete part1_ptr;
   delete part2_ptr;
   
   // While we are here, update the mean free time (assuming energyDensity1 and 2 have not changed much)
   //setMeanFreeTime(numberDensity1,energyDensity1,numberDensity2,energyDensity2);

}

void Coulomb::GalileanScatter( std::array<Real,3>&  a_deltaU,
                         const std::array<Real,3>&  a_vp1,
                         const std::array<Real,3>&  a_vp2,
                         const Real                 a_den12,
                         const Real                 a_Clog,
                         const Real                 a_dt_sec ) const
{
   CH_TIME("Coulomb::GalileanScatter()");
   
   Real ux = a_vp1[0]-a_vp2[0];
   Real uy = a_vp1[1]-a_vp2[1];
   Real uz = a_vp1[2]-a_vp2[2];
   Real u = sqrt(ux*ux + uy*uy + uz*uz);

   // compute s12 = 2.0*deltasq_var
   Real b90 = m_b90_fact/(u*u); // 90 degree impact parameter [m]
   m_s12 = 2.0*Constants::TWOPI*b90*b90*a_den12*a_Clog*u*Constants::CVAC*a_dt_sec;

   switch (m_angular_scattering) {
      case TAKIZUKA:

      if(m_s12<2.0) { // sample from gaussian distribution  
         const Real delta = sqrt(m_s12/2.0)*MathUtils::randn();
         const Real deltasq = delta*delta;
         m_sinth = 2.0*delta/(1.0+deltasq); 
         m_costh = 1.0 - 2.0*deltasq/(1.0+deltasq);
      } 
      else { // set random polar angle between zero and pi
         const Real theta = Constants::PI*MathUtils::rand();
         m_costh = cos(theta);
         m_sinth = sin(theta);
      }
      break;
      case NANBU:

      setNANBUcosthsinth(m_s12);
      break;
   }

   // set random azimuthal angle
   m_phi = Constants::TWOPI*MathUtils::rand();
   m_cosphi = cos(m_phi);
   m_sinphi = sin(m_phi);

   // define deltaU
   ScatteringUtils::computeDeltaU(a_deltaU,ux,uy,uz,m_costh,m_sinth,m_cosphi,m_sinphi);
               
}

void Coulomb::LorentzScatter( std::array<Real,3>&  a_up1,
                              std::array<Real,3>&  a_up2,
                        const bool                 a_scatter2,
                        const long double          a_mass1,
                        const long double          a_mass2,
                        const Real                 a_den12,
                        const Real                 a_dt_sec ) const
{
   CH_TIME("Coulomb::LorentzScatter()");
   
   // Relativistic scattering method for two particles with weight2 < weight1

   long double gamma1, gamma2, Etot, gammacm, gamma1st, gamma2st;
   long double vcmdotup, vrelst, muRst, upst_fact, upstsq, denom;
   std::array<Real,3> vcm, upst;

   // compute the lab frame total energy
   gamma1 = sqrt(1.0 + a_up1[0]*a_up1[0] + a_up1[1]*a_up1[1] + a_up1[2]*a_up1[2]);
   gamma2 = sqrt(1.0 + a_up2[0]*a_up2[0] + a_up2[1]*a_up2[1] + a_up2[2]*a_up2[2]);
   Etot = gamma1*a_mass1 + gamma2*a_mass2;
 
   // compute center-of-momentum beta and gamma
   for(int n=0; n<3; n++) vcm[n] = (a_mass1*a_up1[n] + a_mass2*a_up2[n])/Etot;
   gammacm = 1.0/sqrt(1.0-vcm[0]*vcm[0]-vcm[1]*vcm[1]-vcm[2]*vcm[2]);

   // compute gamma* and up* for particles 1 and 2 ( * = CM frame )
   vcmdotup = vcm[0]*a_up2[0] + vcm[1]*a_up2[1] + vcm[2]*a_up2[2];
   gamma2st = gammacm*(gamma2 - vcmdotup);

   vcmdotup = vcm[0]*a_up1[0] + vcm[1]*a_up1[1] + vcm[2]*a_up1[2];
   gamma1st = gammacm*(gamma1 - vcmdotup);
 
   upst_fact = (gammacm/(1.0+gammacm)*vcmdotup - gamma1)*gammacm;
   for(int n=0; n<3; n++) upst[n] = a_up1[n] + upst_fact*vcm[n]; 
 
   // compute relative beta in CM frame (vrelst = |v1st - v2st|/(1 - v1st/cdotv2st))
   muRst = gamma1st*a_mass1*gamma2st*a_mass2/(a_mass2*gamma2st + a_mass1*gamma1st); 
   upstsq = upst[0]*upst[0] + upst[1]*upst[1] + upst[2]*upst[2];
   denom = 1.0 + upstsq*a_mass1/a_mass2/gamma1st/gamma2st;
   vrelst = sqrt(upstsq)*a_mass1/muRst/denom;

   // compute s12 = 2*<delta^2>
   m_s12 = Constants::PI*m_b90_fact*m_b90_fact*a_den12*m_Clog*vrelst*Constants::CVAC*a_dt_sec;
   m_s12 *= gamma1st*gamma2st/gamma1/gamma2;
   m_s12 /= pow(muRst*vrelst*vrelst,2);
   
   switch (m_angular_scattering) {
      case TAKIZUKA:

      if(m_s12<2.0) { // sample from gaussian distribution  
         const Real delta = sqrt(m_s12/2.0)*MathUtils::randn();
         const Real deltasq = delta*delta;
         m_sinth = 2.0*delta/(1.0+deltasq); 
         m_costh = 1.0 - 2.0*deltasq/(1.0+deltasq);
      } 
      else { // set random polar angle between zero and pi
         const Real theta = Constants::PI*MathUtils::rand();
         m_costh = cos(theta);
         m_sinth = sin(theta);
      }
      break;
      case NANBU:

      setNANBUcosthsinth(m_s12);
      break;
   }
 
   // set random azimuthal angle
   m_phi = Constants::TWOPI*MathUtils::rand();
   m_cosphi = cos(m_phi);
   m_sinphi = sin(m_phi);

   // rotate upst for particle 1 by scattering angles
   ScatteringUtils::rotateVelocity(upst,m_costh,m_sinth,m_cosphi,m_sinphi);

   // Lorentz transform particle 1 back to lab frame
   vcmdotup = vcm[0]*upst[0] + vcm[1]*upst[1] + vcm[2]*upst[2];
   upst_fact = (gammacm/(1.0+gammacm)*vcmdotup + gamma1st)*gammacm;
   for(int n=0; n<3; n++) a_up1[n] = upst[n] + upst_fact*vcm[n]; 
  
   if(a_scatter2) { 
     // Lorentz transform particle 2 back to lab frame (pp2st + pp1st = 0)
      for(int n=0; n<3; n++) upst[n] *= -a_mass1/a_mass2;
      vcmdotup *= -a_mass1/a_mass2;
      upst_fact = (gammacm/(1.0+gammacm)*vcmdotup + gamma2st)*gammacm;
      for(int n=0; n<3; n++) a_up2[n] = upst[n] + upst_fact*vcm[n];
      // set velocity of particle 2 using momentum conservation
      //for(int n=0; n<3; n++) a_up2[n] = (vcm[n]*Etot - a_mass1*a_up1[n])/a_mass2;
   }
 
}

#include "NamespaceFooter.H"

