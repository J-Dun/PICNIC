
#include "DomainGrid.H"
#include "DomainGridF_F.H"
#include "ParmParse.H"
#include <array>
#include <cmath>
#include "BoxIterator.H"
#include "SpaceUtils.H"
#include "PicnicConstants.H"

#include "NamespaceHeader.H"

DomainGrid::DomainGrid( const ProblemDomain&      a_domain,
                        const DisjointBoxLayout&  a_grids,
                        const int                 a_numGhosts,
                        const Real                a_length_scale )
   : m_axisymmetric(false),
     m_anticyclic(false),
     m_write_jacobians(false),
     m_write_corrected_jacobians(false),
     m_ghosts(a_numGhosts),
     m_volume_correction(CONSERVATIVE),
     m_mapped_cell_volume(1.0)
{
   m_grids  = a_grids;
   m_domain = a_domain;
   IntVect dimensions = a_domain.size(); 
   
   // standard ghost exchange copier
   m_forwardCopier.define( m_grids, m_grids, 
                           m_domain, m_ghosts*IntVect::Unit, true );

   // a reversed version of the above
   m_reverseCopier.define( m_grids, m_grids, 
                           m_domain, m_ghosts*IntVect::Unit, true );
   m_reverseCopier.reverse();
   
   ParmParse ppgrid( "grid" );
   ppgrid.query( "write_jacobians", m_write_jacobians );
   ppgrid.query( "write_corrected_jacobians", m_write_corrected_jacobians );

   ppgrid.get( "geometry", m_geom_type );
   if(m_geom_type=="cartesian") {

      ppgrid.get("X_min", m_Xmin[0]);
      ppgrid.get("X_max", m_Xmax[0]);
      m_dX[0] = (m_Xmax[0] - m_Xmin[0])/(double)dimensions[0];    

      if(SpaceDim==2) {
         ppgrid.get("Z_min", m_Xmin[1]);
         ppgrid.get("Z_max", m_Xmax[1]);
         m_dX[1] = (m_Xmax[1] - m_Xmin[1])/(double)dimensions[1];    
      }
      if(SpaceDim==3) {
         ppgrid.get("Y_min", m_Xmin[1]);
         ppgrid.get("Y_max", m_Xmax[1]);
         m_dX[1] = (m_Xmax[1] - m_Xmin[1])/(double)dimensions[1];    
         ppgrid.get("Z_min", m_Xmin[2]);
         ppgrid.get("Z_max", m_Xmax[2]);
         m_dX[2] = (m_Xmax[2] - m_Xmin[2])/(double)dimensions[2];    
      }
   
   }
   else if(m_geom_type=="cyl_R") {
 
      CH_assert(SpaceDim==1);
      m_axisymmetric = true;
      
      if(ppgrid.contains("X_min")) ppgrid.get("X_min", m_Xmin[0]);
      else ppgrid.get("R_min", m_Xmin[0]);
      if(ppgrid.contains("X_max")) ppgrid.get("X_max", m_Xmax[0]);
      else ppgrid.get("R_max", m_Xmax[0]);
      
      m_dX[0] = (m_Xmax[0] - m_Xmin[0])/(double)dimensions[0];    

   }
   else if(m_geom_type=="cyl_RTH") {
 
      CH_assert(SpaceDim==2);

      ppgrid.get("R_min", m_Xmin[0]);
      ppgrid.get("R_max", m_Xmax[0]);
      m_dX[0] = (m_Xmax[0] - m_Xmin[0])/(double)dimensions[0];    
      
      ppgrid.get("TH_min", m_Xmin[1]);
      ppgrid.get("TH_max", m_Xmax[1]);
      m_dX[1] = (m_Xmax[1] - m_Xmin[1])/(double)dimensions[1];

   }
   else if(m_geom_type=="cyl_RZ") {
 
      CH_assert(SpaceDim==2);
      m_axisymmetric = true;
      m_anticyclic = true;

      if(ppgrid.contains("X_min") && ppgrid.contains("X_max")) {
         ppgrid.get("X_min", m_Xmin[0]);
         ppgrid.get("X_max", m_Xmax[0]);
      }
      else {
         ppgrid.get("R_min", m_Xmin[0]);
         ppgrid.get("R_max", m_Xmax[0]);
      }
      CH_assert(m_Xmin[0]>=0.0 && m_Xmax[0]>m_Xmin[0]);
      m_dX[0] = (m_Xmax[0] - m_Xmin[0])/(double)dimensions[0];    
      
      ppgrid.get("Z_min", m_Xmin[1]);
      ppgrid.get("Z_max", m_Xmax[1]);
      CH_assert(m_Xmax[1]>m_Xmin[1]);
      m_dX[1] = (m_Xmax[1] - m_Xmin[1])/(double)dimensions[1];

   }
   else {
      if(!procID()) cout << "m_geom_type " << m_geom_type << " not supported " << endl;
      exit(EXIT_FAILURE);
   }

   // compute mapped cell volume and face areas
   for (int dir=0; dir<SpaceDim; ++dir) {
      m_mapped_cell_volume *= m_dX[dir];
      m_mapped_face_area[dir] = 1.0;
      for(int tdir=0; tdir<SpaceDim; ++tdir) {
         if (tdir != dir) m_mapped_face_area[dir] *= m_dX[tdir];
      }
   }

   if(ppgrid.contains("axisymmetric")) { // can manually turn this on/off
      ppgrid.get("axisymmetric", m_axisymmetric);
      if(m_axisymmetric && SpaceDim==3) {
         MayDay::Error("DomainGrid(): Cannot specify axisymmetry in 3D");
      }
   }
   
   // set the volume/area scales for converting from code to SI unit
   m_volume_scale = a_length_scale;
   for (int dir=1; dir<SpaceDim; dir++) m_volume_scale *= a_length_scale; 
   if(m_axisymmetric) m_volume_scale *= a_length_scale;   
   m_area_scale = m_volume_scale/a_length_scale;
   
   std::string volume_correction;
   ppgrid.query( "volume_correction", volume_correction );   
   if(!volume_correction.empty()) {
      if(volume_correction=="none") {
         m_volume_correction = NONE;
      }
      else if(volume_correction=="verboncoeur") { //see J.P. Verboncoeur JCP 2001
         m_volume_correction = VERBONCOEUR;
	 CH_assert(m_axisymmetric);
      }
      else if(volume_correction=="conservative") { // for theta implicit
         m_volume_correction = CONSERVATIVE;
      }
      else {
         if(!procID()) {
            cout << "DomainGrid: volume_correction = " << volume_correction;
            cout << " is not valid." << endl;
            cout << " valid options: none, verboncoeur, and conservative" << endl;
            exit(EXIT_FAILURE);
         }
      }
   }
   
   int grid_verbosity;
   ppgrid.query( "verbosity", grid_verbosity );
   if(!procID() && grid_verbosity) {
      //cout << "====================== Spatial Grid Parameters =====================" << endl;
      cout << " geometry = " << m_geom_type << endl;
      cout << " axisymmetric = " << m_axisymmetric << endl;
      cout << " anticyclic   = " << m_anticyclic << endl;
      cout << " volume_correction  = " << m_volume_correction << endl;
      cout << "  X_min, X_max = " << m_Xmin[0] << ", " << m_Xmax[0] << endl;
      cout << "  dX = " << m_dX[0] << endl;
      if(SpaceDim==2) {
          cout << "  Z_min, Z_max = " << m_Xmin[1] << ", " << m_Xmax[1] << endl;
          cout << "  dZ = " << m_dX[1] << endl;
      }
      if(SpaceDim==3) {
         cout << "  Y_min, Y_max = " << m_Xmin[1] << ", " << m_Xmax[1] << endl;
         cout << "  dY = " << m_dX[1] << endl;
         cout << "  Z_min, Z_max = " << m_Xmin[2] << ", " << m_Xmax[2] << endl;
         cout << "  dZ = " << m_dX[2] << endl;
      }
      cout << "====================================================================" << endl;
      cout << endl;
   }

   // set the physical coordinates
   setRealCoords();
   
   // set the Jacobian
   setJacobian();

   // define the vector of boundary box layouts for BCs
   defineBoundaryBoxLayout();
}

void DomainGrid::setRealCoords()
{
   IntVect ghostVect = m_ghosts*IntVect::Unit;
   m_Xcc.define(m_grids,SpaceDim,ghostVect);
   m_Xfc.define(m_grids,SpaceDim,ghostVect);
   m_Xec.define(m_grids,SpaceDim,ghostVect);
   m_Xnc.define(m_grids,SpaceDim,ghostVect);

   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      
      // set the coords at cell center
      FORT_GET_CC_MAPPED_COORDS( CHF_BOX(m_Xcc[dit].box()),
                                 CHF_CONST_REALVECT(m_dX),
                                 CHF_FRA(m_Xcc[dit]) );

      for (int dir=0; dir<SpaceDim; ++dir) {
         m_Xcc[dit].plus(m_Xmin[dir],dir,1);
      }
   
      // set the coords at cell faces
      for (int dir=0; dir<SpaceDim; ++dir) {
         FORT_GET_FC_MAPPED_COORDS( CHF_BOX(m_Xfc[dit][dir].box()),
                                    CHF_CONST_INT(dir),
                                    CHF_CONST_REALVECT(m_dX),
                                    CHF_FRA(m_Xfc[dit][dir]) );
      
         for (int tdir=0; tdir<SpaceDim; ++tdir) {
            m_Xfc[dit][dir].plus(m_Xmin[tdir],tdir,1);
         }
      }
       
      // set the coords at cell edges
      for (int dir=0; dir<SpaceDim; ++dir) {
         FORT_GET_EC_MAPPED_COORDS( CHF_BOX(m_Xec[dit][dir].box()),
                                    CHF_CONST_INT(dir),
                                    CHF_CONST_REALVECT(m_dX),
                                    CHF_FRA(m_Xec[dit][dir]) );
      
         for (int tdir=0; tdir<SpaceDim; ++tdir) {
            m_Xec[dit][dir].plus(m_Xmin[tdir],tdir,1);
         }
      }
      //if(!procID()) cout << "JRA: m_Xcc.box() = " << m_Xcc[dit].box() << endl;      
      //if(!procID()) cout << "JRA: m_Xnc.box() = " << m_Xnc[dit].box() << endl;      
      //if(!procID()) cout << "JRA: m_Xnc.getFab().box() = " << m_Xnc[dit].getFab().box() << endl;      
      // set the coords at cell nodes
      FORT_GET_NC_MAPPED_COORDS( CHF_BOX(surroundingNodes(m_Xnc[dit].box())),
                                 CHF_CONST_REALVECT(m_dX),
                                 CHF_FRA(m_Xnc[dit]) );
      
      for (int dir=0; dir<SpaceDim; ++dir) {
         m_Xnc[dit].plus(m_Xmin[dir],dir,1);
      }

   }
   
}

void DomainGrid::setJacobian()
{
   const Real Pi = Constants::PI; 
   const Real twoPi = Constants::TWOPI; 
   IntVect ghostVect = m_ghosts*IntVect::Unit;

   //
   // define point-wise Jacobians
   //

   m_Jcc.define(m_grids,1,ghostVect);
   m_Jfc.define(m_grids,1,ghostVect);
   m_Jec.define(m_grids,1,ghostVect);
   m_Jnc.define(m_grids,1,ghostVect);
   
   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      m_Jcc[dit].setVal(1.0); 
      for(int dir=0; dir<SpaceDim; dir++) {
         m_Jfc[dit][dir].setVal(1.0);
         m_Jec[dit][dir].setVal(1.0);
      }
      m_Jnc[dit].getFab().setVal(1.0); 
   }

   if(m_geom_type=="cyl_RTH") {

      for(DataIterator dit(m_grids); dit.ok(); ++dit) {
         m_Jcc[dit].mult(m_Xcc[dit],0,0,1); 
         for(int dir=0; dir<SpaceDim; dir++) {
            m_Jfc[dit][dir].mult(m_Xfc[dit][dir],0,0,1);
            m_Jec[dit][dir].mult(m_Xec[dit][dir],0,0,1);
         }
         m_Jnc[dit].getFab().mult(m_Xnc[dit].getFab(),0,0,1); 
      }

   }
   
   if( m_axisymmetric ) {

      for(DataIterator dit(m_grids); dit.ok(); ++dit) {
         m_Jcc[dit].mult(m_Xcc[dit],0,0,1); 
         m_Jcc[dit].mult(twoPi,0,1);
         for(int dir=0; dir<SpaceDim; dir++) {
            m_Jfc[dit][dir].mult(m_Xfc[dit][dir],0,0,1);
            m_Jfc[dit][dir].mult(twoPi,0,1);
            m_Jec[dit][dir].mult(m_Xec[dit][dir],0,0,1);
            m_Jec[dit][dir].mult(twoPi,0,1);
         }
         m_Jnc[dit].getFab().mult(m_Xnc[dit].getFab(),0,0,1); 
         m_Jnc[dit].getFab().mult(twoPi,0,1);
      }

   }
   
   //
   // define boundary-corrected nodal Jacobians used for 
   // charge/current density deposit and for volume integrals
   //

   m_corrected_Jfc.define(m_grids,1,IntVect::Zero);
   m_corrected_Jec.define(m_grids,1,IntVect::Zero);
   m_corrected_Jnc.define(m_grids,1,IntVect::Zero);

   // first define via copy
   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      for(int dir=0; dir<SpaceDim; dir++) {
         m_corrected_Jfc[dit][dir].copy(m_Jfc[dit][dir]);
         m_corrected_Jec[dit][dir].copy(m_Jec[dit][dir]);
      }
      m_corrected_Jnc[dit].getFab().copy(m_Jnc[dit].getFab()); 
   }

   // redefine J on physical boundaries for non-periodic domains
   auto phys_domain( m_grids.physDomain() );
   
   if(!m_domain.isPeriodic(0)) {
      
      for(DataIterator dit(m_grids); dit.ok(); ++dit) {

         // get the domain box on nodes
         Box domain_node_box = phys_domain.domainBox();
         domain_node_box.surroundingNodes();
         int dir0 = 0;
         int dir0_bdry_hi = domain_node_box.bigEnd(dir0);
         int dir0_bdry_lo = domain_node_box.smallEnd(dir0);

         // get Jacobian on nodes and define local node box
         FArrayBox& this_Jnc(m_corrected_Jnc[dit].getFab());
         Box node_box = m_grids[dit];
         node_box.surroundingNodes();

         Real local_J;
         IntVect ig;
         
         if(m_volume_correction==VERBONCOEUR) {
            IntVect shift_vect = IntVect::Zero;
            shift_vect[dir0] = 1;
            BoxIterator gbit(node_box);
            for(gbit.begin(); gbit.ok(); ++gbit) {
               ig = gbit(); // grid index
               Real rj = m_Xnc[dit].getFab().get(ig,0);
               Real rjp1 = m_Xnc[dit].getFab().get(ig+shift_vect,0);
               Real rjm1 = m_Xnc[dit].getFab().get(ig-shift_vect,0);
               if(ig[dir0]==dir0_bdry_lo) {
                  local_J = Pi/3.0*(rjp1 - rj)*(2*rj + rjp1)/m_dX[dir0];
               }
               else if(ig[dir0]==dir0_bdry_hi) {
                  local_J = Pi/3.0*(rj - rjm1)*(2*rjm1 + 2*rj)/m_dX[dir0];
               }
               else {
                  local_J = Pi/3.0*(rjp1*(rj + rjp1) - rjm1*(rjm1 + rj))/m_dX[dir0];
               }
               this_Jnc.set(ig,0,local_J);
            }
         }
         else if(m_volume_correction==CONSERVATIVE) {
            BoxIterator gbit(node_box);
            for(gbit.begin(); gbit.ok(); ++gbit) {
               ig = gbit(); // grid index
               Real rj = m_Xnc[dit].getFab().get(ig,0);
               if(ig[dir0]==dir0_bdry_lo) {
                  if(m_axisymmetric && rj==0.0) {
		     local_J = Pi*m_dX[dir0]/4.0;
                     this_Jnc.set(ig,0,local_J);
		  }
                  else this_Jnc.set(ig,0,0.5*this_Jnc.get(ig,0));
               }
               if(ig[dir0]==dir0_bdry_hi) {
                  this_Jnc.set(ig,0,0.5*this_Jnc.get(ig,0));
               }
            }
         }

      }

      for(DataIterator dit(m_grids); dit.ok(); ++dit) {

         // get the domain box on face
         const int dir0=0;
         Box domain_face_box = phys_domain.domainBox();
         domain_face_box.surroundingNodes(dir0);
         int dir0_bdry_hi = domain_face_box.bigEnd(dir0);
         int dir0_bdry_lo = domain_face_box.smallEnd(dir0);

         // get Jacobian on faces and define local face box
         FArrayBox& this_Jfc(m_corrected_Jfc[dit][dir0]);
         Box face_box = m_grids[dit];
         face_box.surroundingNodes(dir0);

         Real local_J;
         IntVect ig;

         if(m_volume_correction==VERBONCOEUR) {
            IntVect shift_vect = IntVect::Zero;
            shift_vect[dir0] = 1;
            BoxIterator gbit(face_box);
            for(gbit.begin(); gbit.ok(); ++gbit) {
               ig = gbit(); // grid index
               Real rj = m_Xfc[dit][dir0].get(ig,0);
               Real rjp1 = m_Xfc[dit][dir0].get(ig+shift_vect,0);
               Real rjm1 = m_Xfc[dit][dir0].get(ig-shift_vect,0);
               if(ig[dir0]==dir0_bdry_lo) {
                  local_J = Pi/3.0*(rjp1 - rj)*(2*rj + rjp1)/m_dX[dir0];
               }
               else if(ig[dir0]==dir0_bdry_hi) {
                  local_J = Pi/3.0*(rj - rjm1)*(2*rjm1 + 2*rj)/m_dX[dir0];
               }
               else {
                  local_J = Pi/3.0*(rjp1*(rj + rjp1) - rjm1*(rjm1 + rj))/m_dX[dir0];
               }
               this_Jfc.set(ig,0,local_J);
            }
         }
         else if(m_volume_correction==CONSERVATIVE) {
            BoxIterator gbit(face_box);
            for(gbit.begin(); gbit.ok(); ++gbit) {
               ig = gbit(); // grid index
               Real rj = m_Xfc[dit][dir0].get(ig,0);
               if(ig[dir0]==dir0_bdry_lo) {
                  if(m_axisymmetric && rj==0.0) {
		     local_J = Pi*m_dX[dir0]/4.0;
                     this_Jfc.set(ig,0,local_J);
		  }
                  else this_Jfc.set(ig,0,0.5*this_Jfc.get(ig,0));
               }
               if(ig[dir0]==dir0_bdry_hi) {
                  this_Jfc.set(ig,0,0.5*this_Jfc.get(ig,0));
               }
            }
         }

      }

   }
   
#if CH_SPACEDIM==2
   if(!m_domain.isPeriodic(1)) {
      
      for(DataIterator dit(m_grids); dit.ok(); ++dit) {
         const int dir0=0;
         const int dir1=1;

         // get the domain box on edges
         Box domain_edge_box = phys_domain.domainBox();
         domain_edge_box.surroundingNodes();
         domain_edge_box.enclosedCells(dir1);
         int dir0_bdry_hi = domain_edge_box.bigEnd(dir0);
         int dir0_bdry_lo = domain_edge_box.smallEnd(dir0);

         // get Jacobian on edges and define local edge box
         FArrayBox& this_Jec(m_corrected_Jec[dit][dir1]);
         Box edge_box = m_grids[dit];
         edge_box.surroundingNodes();
         edge_box.enclosedCells(dir1);

         Real local_J;
         IntVect ig;
         
         if(m_volume_correction==VERBONCOEUR) {
            IntVect shift_vect = IntVect::Zero;
            shift_vect[dir0] = 1;
            BoxIterator gbit(edge_box);
            for(gbit.begin(); gbit.ok(); ++gbit) {
               ig = gbit(); // grid index
               Real rj = m_Xec[dit][dir1].get(ig,0);
               Real rjp1 = m_Xec[dit][dir1].get(ig+shift_vect,0);
               Real rjm1 = m_Xec[dit][dir1].get(ig-shift_vect,0);
               if(ig[dir0]==dir0_bdry_lo) {
                  local_J = Pi/3.0*(rjp1 - rj)*(2*rj + rjp1)/m_dX[dir0];
               }
               else if(ig[dir0]==dir0_bdry_hi) {
                  local_J = Pi/3.0*(rj - rjm1)*(2*rjm1 + 2*rj)/m_dX[dir0];
               }
               else {
                  local_J = Pi/3.0*(rjp1*(rj + rjp1) - rjm1*(rjm1 + rj))/m_dX[dir0];
               }
               this_Jec.set(ig,0,local_J);
            }
         }
         else if(m_volume_correction==CONSERVATIVE) {
            BoxIterator gbit(edge_box);
            for(gbit.begin(); gbit.ok(); ++gbit) {
               ig = gbit(); // grid index
               Real rj = m_Xec[dit][dir1].get(ig,0);
               if(ig[dir0]==dir0_bdry_lo) {
                  if(m_axisymmetric && rj==0.0) {
	             local_J = Pi*m_dX[dir0]/4.0;
                     this_Jec.set(ig,0,local_J);
		  }
                  else this_Jec.set(ig,0,0.5*this_Jec.get(ig,0));
               }
               if(ig[dir0]==dir0_bdry_hi) {
                  this_Jec.set(ig,0,0.5*this_Jec.get(ig,0));
               }
            }
         }

      }

   }
#endif

   //
   // define masked nodal Jacobians that include scale factors 
   // on shared nodal locations needed to get volume/area integrals
   // correct when doing MPI sum (e.g., if the nodal location is shared
   // by two boxes, then the masked Jacobian gets a 1/2 scale factor)
   // 

   m_masked_Jfc.define(m_grids,1,IntVect::Zero);
   m_masked_Jec.define(m_grids,1,IntVect::Zero);
   m_masked_Jnc.define(m_grids,1,IntVect::Zero);

   // first define via copy
   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      for(int dir=0; dir<SpaceDim; dir++) {
         m_masked_Jfc[dit][dir].copy(m_corrected_Jfc[dit][dir]);
         m_masked_Jec[dit][dir].copy(m_corrected_Jec[dit][dir]);
      }
      m_masked_Jnc[dit].getFab().copy(m_corrected_Jnc[dit].getFab()); 
   }
   
   // mask the face centered Jacobian
   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      for(int dir=0; dir<SpaceDim; dir++) {
         Box face_box = m_grids[dit];
         face_box.surroundingNodes(dir);
         Box internal_box = face_box;
         internal_box.grow(dir,-1);

         // correct internal_box on physical boundaries
         if(!m_domain.isPeriodic(dir)) {
             Box domain_face_box = phys_domain.domainBox();
             domain_face_box.surroundingNodes(dir);
             if(face_box.bigEnd(dir)==domain_face_box.bigEnd(dir)) {
                internal_box.growHi(dir,1);
             }
             if(face_box.smallEnd(dir)==domain_face_box.smallEnd(dir)) {
                internal_box.growLo(dir,1);
             }
         }
         
         FArrayBox mask(face_box,1);
         mask.setVal(0.5);
         mask.plus(0.5,internal_box,0,1); 

         m_masked_Jfc[dit][dir].mult(mask);
      }
   }
   
   // mask the edge centered Jacobian
   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      for(int dir=0; dir<SpaceDim; dir++) {
         Box edge_box = m_grids[dit];
         edge_box.surroundingNodes();
         edge_box.enclosedCells(dir);
         Box internal_box = edge_box;
         for (int adir=0; adir<SpaceDim; adir++) {
            if(adir==dir) continue;
            internal_box.grow(adir,-1);
         }
         
         // correct internal_box on physical boundaries
         if(!m_domain.isPeriodic(dir)) {
             Box domain_edge_box = phys_domain.domainBox();
             domain_edge_box.surroundingNodes();
             domain_edge_box.enclosedCells(dir);
             for (int adir=0; adir<SpaceDim; adir++) {
                if (adir==dir) continue;
                if(edge_box.bigEnd(adir)==domain_edge_box.bigEnd(adir)) {
                   internal_box.growHi(adir,1);
                }
                if(edge_box.smallEnd(adir)==domain_edge_box.smallEnd(adir)) {
                   internal_box.growLo(adir,1);
                }
             }
         }

         FArrayBox mask(edge_box,1);
#if CH_SPACEDIM==1
         mask.setVal(1.0);
#elif CH_SPACEDIM==2
         mask.setVal(0.5);
         mask.plus(0.5,internal_box,0,1); 
#elif CH_SPACEDIM==3
         mask.setVal(0.25);
         mask.plus(0.75,internal_box,0,1); 
#endif
         m_masked_Jec[dit][dir].mult(mask);
      }
   }
   
   // mask the node centered Jacobian
   for(DataIterator dit(m_grids); dit.ok(); ++dit) {
      Box node_box = m_grids[dit];
      node_box.surroundingNodes();
      Box internal_box = node_box;
      for (int dir=0; dir<SpaceDim; dir++) internal_box.grow(dir,-1);
         
      // correct internal_box on physical boundaries
      // Is the correct for SpaceDim > 1 ?...
      Box domain_node_box = phys_domain.domainBox();
      domain_node_box.surroundingNodes();
      for (int dir=0; dir<SpaceDim; dir++) {
         if(m_domain.isPeriodic(dir)) continue;
         if(node_box.bigEnd(dir)==domain_node_box.bigEnd(dir)) {
            internal_box.growHi(dir,1);
         }
         if(node_box.smallEnd(dir)==domain_node_box.smallEnd(dir)) {
            internal_box.growLo(dir,1);
         }
      }

      FArrayBox mask(node_box,1);
#if CH_SPACEDIM==1
      mask.setVal(0.5);
      mask.plus(0.5,internal_box,0,1);
#elif CH_SPACEDIM==2 // corners get 1/4, edges get 1/2
      mask.setVal(0.25);
      for(int dir=0; dir<SpaceDim; dir++) {
         Box node_box0 = node_box;
         node_box0.grow(dir,-1);
         mask.plus(0.25,node_box0,0,1);
      }
      mask.plus(0.25,internal_box,0,1);
#elif CH_SPACEDIM==3  // corners get 1/8, edges get 1/4, faces get 1/2
      mask.setVal(0.125);
      for(int dir=0; dir<SpaceDim; dir++) {
         Box node_box0 = node_box;
         node_box0.grow(dir,-1);
         mask.plus(0.125,node_box0,0,1);
         //
         Box internal_box0 = internal_box;
         internal_box0.grow(dir,1);
         mask.plus(0.125,internal_box0,0,1);
      }
      mask.plus(0.125,internal_box,0,1);
#endif

      m_masked_Jnc[dit].getFab().mult(mask);
   }
   
   //   display the Jacobian values for verification
   //if(procID()==0) displayJacobianData();

}

void DomainGrid::displayJacobianData() {

   for(DataIterator dit(m_grids); dit.ok(); ++dit) {

      cout << "procID() = " << procID() << endl;
            
      Box Jnc_box = m_Jnc[dit].getFab().box();
      cout << "JRA: m_Jnc.box() = " << Jnc_box << endl;      
      BoxIterator gbit(Jnc_box);
      for(gbit.begin(); gbit.ok(); ++gbit) {
         IntVect ig = gbit(); // grid index
         Real localJ = m_Jnc[dit].getFab().get(ig,0);
	 cout << "JRA: Jnc(ig="<<ig<<") = " << localJ << endl;
      }
      cout << endl;

      Box corrected_Jnc_box = m_corrected_Jnc[dit].getFab().box();
      cout << "JRA: m_corrected_Jnc.box() = " << corrected_Jnc_box << endl;      
      BoxIterator gbit2(corrected_Jnc_box);
      for(gbit2.begin(); gbit2.ok(); ++gbit2) {
         IntVect ig = gbit2(); // grid index
         Real localJ = m_corrected_Jnc[dit].getFab().get(ig,0);
	 cout << "JRA: corrected Jnc(ig="<<ig<<") = " << localJ << endl;
      }
      cout << endl;
      
      Box masked_Jnc_box = m_masked_Jnc[dit].getFab().box();
      cout << "JRA: m_masked_Jnc.box() = " << masked_Jnc_box << endl;      
      BoxIterator gbit3(masked_Jnc_box);
      for(gbit3.begin(); gbit3.ok(); ++gbit3) {
         IntVect ig = gbit3(); // grid index
         Real localJ = m_masked_Jnc[dit].getFab().get(ig,0);
	 cout << "JRA: masked Jnc(ig="<<ig<<") = " << localJ << endl;
      }
      cout << endl;

      for (int dir=0; dir<SpaceDim; dir++) {
	 cout << "JRA: dir = " << dir << endl;
         Box Jfc_box = m_Jfc[dit][dir].box();
         cout << "JRA: m_Jfc.box() = " << Jfc_box << endl;      
         BoxIterator gbit(Jfc_box);
         for(gbit.begin(); gbit.ok(); ++gbit) {
            IntVect ig = gbit(); // grid index
            Real localJ = m_Jfc[dit][dir].get(ig,0);
	    cout << "JRA: Jfc(ig="<<ig<<",dir="<<dir<<") = " << localJ << endl;
         }
         cout << endl;

	 Box corrected_Jfc_box = m_corrected_Jfc[dit][dir].box();
         cout << "JRA: m_corrected_Jfc.box() = " << corrected_Jfc_box << endl;      
         BoxIterator gbit2(corrected_Jfc_box);
         for(gbit2.begin(); gbit2.ok(); ++gbit2) {
            IntVect ig = gbit2(); // grid index
            Real localJ = m_corrected_Jfc[dit][dir].get(ig,0);
	    cout << "JRA: Jfc(ig="<<ig<<",dir="<<dir<<") = " << localJ << endl;
         }
         cout << endl;
	 
	 Box masked_Jfc_box = m_masked_Jfc[dit][dir].box();
         cout << "JRA: m_masked_Jfc.box() = " << masked_Jfc_box << endl;      
         BoxIterator gbit3(masked_Jfc_box);
         for(gbit3.begin(); gbit3.ok(); ++gbit3) {
            IntVect ig = gbit3(); // grid index
            Real localJ = m_masked_Jfc[dit][dir].get(ig,0);
	    cout << "JRA: Jfc(ig="<<ig<<",dir="<<dir<<") = " << localJ << endl;
         }
         cout << endl;

      }

      for (int dir=0; dir<SpaceDim; dir++) {
	 cout << "JRA: dir = " << dir << endl;
         Box Jec_box = m_Jec[dit][dir].box();
         cout << "JRA: m_Jec.box() = " << Jec_box << endl;      
         BoxIterator gbit(Jec_box);
         for(gbit.begin(); gbit.ok(); ++gbit) {
            IntVect ig = gbit(); // grid index
            Real localJ = m_Jec[dit][dir].get(ig,0);
	    cout << "JRA: Jec(ig="<<ig<<",dir="<<dir<<") = " << localJ << endl;
         }
         cout << endl;

	 Box corrected_Jec_box = m_corrected_Jec[dit][dir].box();
         cout << "JRA: m_corrected_Jec.box() = " << corrected_Jec_box << endl;      
         BoxIterator gbit2(corrected_Jec_box);
         for(gbit2.begin(); gbit2.ok(); ++gbit2) {
            IntVect ig = gbit2(); // grid index
            Real localJ = m_corrected_Jec[dit][dir].get(ig,0);
	    cout << "JRA: Jec(ig="<<ig<<",dir="<<dir<<") = " << localJ << endl;
         }
         cout << endl;
	 
	 Box masked_Jec_box = m_masked_Jec[dit][dir].box();
         cout << "JRA: m_masked_Jec.box() = " << masked_Jec_box << endl;      
         BoxIterator gbit3(masked_Jec_box);
         for(gbit3.begin(); gbit3.ok(); ++gbit3) {
            IntVect ig = gbit3(); // grid index
            Real localJ = m_masked_Jec[dit][dir].get(ig,0);
	    cout << "JRA: Jec(ig="<<ig<<",dir="<<dir<<") = " << localJ << endl;
         }
         cout << endl;

      }

   }

}

void DomainGrid::defineBoundaryBoxLayout()
{

   const IntVect ghostVect = m_ghosts*IntVect::Unit;
   const Box domain_box = m_domain.domainBox();

   for(int dir=0; dir<SpaceDim; dir++) {
      if(m_domain.isPeriodic(dir)) {
         for(SideIterator si; si.ok(); ++si) {
            Side::LoHiSide side( si() );
            m_periodic_bdry_layout.push_back(
            BoundaryBoxLayoutPtr( new BoundaryBoxLayout( m_grids,
                                                         domain_box,
                                                         dir,
                                                         side,
                                                         ghostVect )));
         }
      }
      else {
         for(SideIterator si; si.ok(); ++si) {
            Side::LoHiSide side( si() );
            m_domain_bdry_layout.push_back(
            BoundaryBoxLayoutPtr( new BoundaryBoxLayout( m_grids,
                                                         domain_box,
                                                         dir,
                                                         side,
                                                         ghostVect )));
         }
      }
   }

}


#include "NamespaceFooter.H"

