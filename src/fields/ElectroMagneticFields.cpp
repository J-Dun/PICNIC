#include <array>
#include <cmath>
#include "PicnicConstants.H"
#include "BoxIterator.H"
#include "CH_HDF5.H"
#include "MathUtils.H"
#include "ElectroMagneticFields.H"
#include "FieldsF_F.H"

#include "NamespaceHeader.H"

ElectroMagneticFields::ElectroMagneticFields( ParmParse&    a_ppflds,
                                        const DomainGrid&   a_mesh,
                                        const CodeUnits&    a_units,
                                        const bool&         a_verbosity,
                                        const EMVecType&    a_vec_type )
    : m_verbosity(a_verbosity),
      m_use_filtering(false),
      m_use_poisson(false),
      m_enforce_gauss_startup(false),
      m_external_fields(false),
      m_writeDivs(false),
      m_writeCurls(false),
      m_writeRho(false),
      m_writeExB(false),
      m_mesh(a_mesh),
      m_field_bc(NULL),
      m_vec_size_bfield(0),
      m_vec_size_bfield_virtual(0),
      m_vec_size_efield(0),
      m_vec_size_efield_virtual(0),
      m_vec_offset_bfield(-1),
      m_vec_offset_bfield_virtual(-1),
      m_vec_offset_efield(-1),
      m_vec_offset_efield_virtual(-1),
      m_pc_mass_matrix_width(1),
      m_pc_mass_matrix_include_ij(false)
{
 
   // parse input file
   a_ppflds.get( "advance", m_advance );
   
   if(m_advance) {
      m_advanceE_inPlane = true;
      m_advanceE_virtual = true;
      m_advanceE_comp = {true,true,true};
      if(a_ppflds.contains("advance_electric_field")) {
         vector<int> this_advanceE_comp(3);
         a_ppflds.getarr( "advance_electric_field", this_advanceE_comp, 0, 3 );
         for (int dim=0; dim<3; dim++) m_advanceE_comp[dim] = (this_advanceE_comp[dim] == 1);
         m_advanceE_inPlane = false;
         for (int dim=0; dim<SpaceDim; dim++) m_advanceE_inPlane |= m_advanceE_comp[dim];
         m_advanceE_virtual = false;
         for (int dim=SpaceDim; dim<3; dim++) m_advanceE_virtual |= m_advanceE_comp[dim];
      }

      m_advanceB_inPlane = true;
      m_advanceB_virtual = true;
      m_advanceB_comp = {true,true,true};
      if(a_ppflds.contains("advance_magnetic_field")) {
         vector<int> this_advanceB_comp(3);
         a_ppflds.getarr( "advance_magnetic_field", this_advanceB_comp, 0, 3 );
         for (int dim=0; dim<3; dim++) m_advanceB_comp[dim] = (this_advanceB_comp[dim] == 1);
         m_advanceB_inPlane = false;
         for (int dim=0; dim<SpaceDim; dim++) m_advanceB_inPlane |= m_advanceB_comp[dim];
         m_advanceB_virtual = false;
         for (int dim=SpaceDim; dim<3; dim++) m_advanceB_virtual |= m_advanceB_comp[dim];
      }
   }
   else {
      m_advanceE_inPlane = false;
      m_advanceE_virtual = false;
      m_advanceE_comp = {false,false,false};
      m_advanceB_inPlane = false;
      m_advanceB_virtual = false;
      m_advanceB_comp = {false,false,false};
   }   
   m_advanceE = m_advanceE_inPlane;
   m_advanceE |= m_advanceE_virtual;
   m_advanceB = m_advanceB_inPlane;
   m_advanceB |= m_advanceB_virtual;

   a_ppflds.query( "write_divs", m_writeDivs );
   a_ppflds.query( "write_curls", m_writeCurls );
   a_ppflds.query( "write_rho", m_writeRho );
   a_ppflds.query( "write_exb", m_writeExB );

   a_ppflds.query( "pc_mass_matrix_width", m_pc_mass_matrix_width);
   if(m_pc_mass_matrix_width>0) {
      a_ppflds.query( "pc_mass_matrix_include_ij", m_pc_mass_matrix_include_ij);
   }
   
   a_ppflds.query( "use_filtering", m_use_filtering );

   /* setting number of bands in preconditioner matrix */
   int nbands_EMfields = 1 + 2*SpaceDim;
   int nbands_J_mass_matrix = 1 + 2*3*m_pc_mass_matrix_width;
   m_pcmat_nbands = nbands_EMfields + nbands_J_mass_matrix;
   
   // set the Courant time step associated with the speed of light
   const RealVect& meshSpacing(m_mesh.getdX());
   Real invDXsq = 0.0;
   for (int dir=0; dir<SpaceDim; dir++) {
      invDXsq = invDXsq + 1.0/meshSpacing[dir]/meshSpacing[dir];
   } 
   Real DXeff = 1.0/pow(invDXsq,0.5);
   m_stable_dt = DXeff/a_units.CvacNorm();

   // set the normalization factor for J in Ampere's law
   const Real mu0  = Constants::MU0;  // SI units
   const Real qe   = Constants::QE;   // SI units
   const Real cvac = Constants::CVAC; // SI units
   const Real Xscale = a_units.getScale(a_units.LENGTH);
   const Real Bscale = a_units.getScale(a_units.MAGNETIC_FIELD);
   m_Jnorm_factor = mu0*qe*cvac*Xscale/Bscale;
   
   const Real ep0  = Constants::EP0;  // SI units
   const Real Escale = a_units.getScale(a_units.ELECTRIC_FIELD);
   m_rhoCnorm_factor = qe/ep0*Xscale/Escale;
   
   const Real volume_scale = m_mesh.getVolumeScale();
   const Real dVolume = m_mesh.getMappedCellVolume()*volume_scale; // SI units
   m_energyE_factor = 0.5*Escale*Escale*ep0*dVolume;
   m_energyB_factor = 0.5*Bscale*Bscale/mu0*dVolume;
   
   const Real area_scale = m_mesh.getAreaScale();
   m_SdA_factor = Escale*Bscale/mu0*area_scale;
   m_time_scale = a_units.getScale(a_units.TIME);
   
   // define profiles for external fields
   a_ppflds.query( "external_fields", m_external_fields );
   if(m_external_fields) initExternalFields();

   //
   // define the member LevelDatas
   //
  
   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const int ghosts(m_mesh.ghosts());
   const IntVect ghostVect = ghosts*IntVect::Unit; 
   
   // define the magnetic field containers
   m_magneticField.define(grids,1,ghostVect); // FluxBox with 1 comp
   m_magneticField_old.define(grids,1);
   m_magneticField_rhs.define(grids,1);
   SpaceUtils::zero( m_magneticField_rhs );
   if(SpaceDim<3) {
      m_magneticField_virtual.define(grids,3-SpaceDim,ghostVect); // FArrayBox
      m_magneticField_virtual_old.define(grids,3-SpaceDim);
      m_magneticField_virtual_rhs.define(grids,3-SpaceDim);
      SpaceUtils::zero( m_magneticField_virtual_rhs );
   }

   // define the electric field containers
   m_electricField.define(grids,1,ghostVect); // EdgeDataBox with 1 comp
   m_electricField_old.define(grids,1,IntVect::Zero);
   m_electricField_rhs.define(grids,1,IntVect::Zero);
   SpaceUtils::zero( m_electricField_rhs );
   if(SpaceDim<3) {
      m_electricField_virtual.define(grids,3-SpaceDim,ghostVect); // NodeFArrayBox
      m_electricField_virtual_old.define(grids,3-SpaceDim,IntVect::Zero);
      m_electricField_virtual_rhs.define(grids,3-SpaceDim,IntVect::Zero);
      SpaceUtils::zero( m_electricField_virtual_rhs );
   }
   
   // define the filtered field containers
   m_electricField_filtered.define(grids,1,ghostVect); // EdgeDataBox with 1 comp
   
   // define the current density containers
   m_currentDensity.define(grids,1,ghostVect);    // EdgeDataBox with 1 comp
   if(SpaceDim<3) {m_currentDensity_virtual.define(grids,3-SpaceDim,ghostVect);} // NodeFArrayBox
   m_chargeDensity.define(grids,1,IntVect::Zero);

   // define the curlE containers
   m_curlE.define(grids,1,IntVect::Zero);     // FluxBox with 1 comp
   if(SpaceDim<3) {m_curlE_virtual.define(grids,3-SpaceDim,IntVect::Zero);} // FArrayBox
   
   // define the curlB containers
   m_curlB.define(grids,1,IntVect::Zero);     // EdgeDataBox with 1 comp
   if(SpaceDim<3) {m_curlB_virtual.define(grids,3-SpaceDim,IntVect::Zero);} // NodeFArrayBox
   
   // define ExB containers (poynting flux)
   m_ExB.define(grids,2,IntVect::Zero);
   if(SpaceDim<3) {m_EvxB.define(grids,6-2*SpaceDim,IntVect::Zero);}

   // set in-plane curl to zero
   SpaceUtils::zero( m_curlE );
   SpaceUtils::zero( m_curlB );
   
   // define the divE and divB containers
   m_divE.define(grids,1,IntVect::Zero);
   m_divB.define(grids,1,IntVect::Zero);
   
   // define pointer to the BC object
   m_field_bc = new FieldBC( m_mesh, m_verbosity );
  
   // define poisson solver
   a_ppflds.query( "use_poisson", m_use_poisson );
   a_ppflds.query( "enforce_gauss_startup", m_enforce_gauss_startup );
   if(m_use_poisson || m_enforce_gauss_startup) {

      // parse the input file for the poisson solver
      getPoissonParameters(m_poisson_params, m_mesh);

      // setup the dbl for the elliptic solver
      int numlevels = m_poisson_params.numLevels;
      //Vector<DisjointBoxLayout> vectGrids(numlevels,grids);
      Vector<DisjointBoxLayout> vectGrids;
      setGrids(vectGrids,  m_poisson_params);

      for (int ilev = 0; ilev < vectGrids.size(); ilev++) { 
         //vectGrids[ilev] = grids;
         if(!procID()) {
            cout << "using poisson corrector" << endl;
            cout << "grid at level " << ilev << " = " << vectGrids[ilev] << endl;
         }
      }

      // define the data containers for the potential solver
      m_phi_vector.resize(numlevels);
      m_rhs_vector.resize(numlevels);
      int ncomp  = 1;
      for (int ilev = 0; ilev < numlevels; ilev++) {
         m_phi_vector[ilev] = new LevelData<NodeFArrayBox>(vectGrids[ilev], ncomp, ghostVect);
         m_rhs_vector[ilev] = new LevelData<NodeFArrayBox>(vectGrids[ilev], ncomp, ghostVect);
      }
      m_electricField_correction.define(grids,1,ghostVect);  // EdgeDataBox with 1 comp
      m_phi0.define(grids,1,ghostVect);
      m_rhs0.define(grids,1,ghostVect);

      // define the elliptic solver
      m_bottomSolver.m_verbosity = 1;
      nodeDefineSolver(m_poisson_solver, vectGrids, m_bottomSolver, m_poisson_params); 
   
   }
   
   if(!procID()) {
      cout << " m_Jnorm_factor = " << m_Jnorm_factor << endl;
      cout << " advance fields = " << m_advance << endl;
      if(m_advance) {
         cout << " advance_E = " << m_advanceE_comp[0] << " " 
                                 << m_advanceE_comp[1] << " " 
                                 << m_advanceE_comp[2] << endl;
         cout << " advance_B = " << m_advanceB_comp[0] << " " 
                                 << m_advanceB_comp[1] << " " 
                                 << m_advanceB_comp[2] << endl;
      }
      cout << " use binomial filtering = " << m_use_filtering << endl;
      cout << " use poisson = " << m_use_poisson << endl;
      cout << " enforce gauss startup = " << m_enforce_gauss_startup << endl;
      cout << " external fields = " << m_external_fields << endl;
      cout << " write curls = " << m_writeCurls << endl;
      cout << " write divs  = " << m_writeDivs << endl;
      cout << " write rho  = " << m_writeRho << endl;
      cout << " write ExB  = " << m_writeExB << endl;
      cout << " stable dt = " << m_stable_dt << endl << endl;
   }
   
   m_vec_size_bfield = SpaceUtils::nDOF( m_magneticField );
   m_vec_size_efield = SpaceUtils::nDOF( m_electricField );
   if (SpaceDim < 3) {
     m_vec_size_bfield_virtual = SpaceUtils::nDOF( m_magneticField_virtual );
     m_vec_size_efield_virtual = SpaceUtils::nDOF( m_electricField_virtual );
   }

   int vec_size_total = 0;

   if ((a_vec_type == e_and_b) || (a_vec_type == b_only)) {

     m_vec_offset_bfield = vec_size_total;
     vec_size_total += m_vec_size_bfield;

     if (SpaceDim < 3) {
       m_vec_offset_bfield_virtual = vec_size_total;
       vec_size_total += m_vec_size_bfield_virtual;
     }

   }

   if ((a_vec_type == e_and_b) || (a_vec_type == e_only)) {
     m_vec_offset_efield = vec_size_total;
     vec_size_total += m_vec_size_efield;

     if (SpaceDim < 3) {
       m_vec_offset_efield_virtual = vec_size_total;
       vec_size_total += m_vec_size_efield_virtual;
     }

   }

   if (a_vec_type == e_and_b) {
      m_gdofs.define(  vec_size_total,
                       m_magneticField,
                       m_magneticField_virtual,
                       m_electricField,
                       m_electricField_virtual );
   } else if (a_vec_type == e_only) {
      m_gdofs.define(  vec_size_total,
                       m_electricField,
                       m_electricField_virtual );
   }

   return;
}

ElectroMagneticFields::~ElectroMagneticFields()
{

   if(m_field_bc!=NULL) {
      delete m_field_bc;
      m_field_bc = NULL;
   }
   for (int n=0; n<m_phi_vector.size(); n++) {
      delete m_phi_vector[n];
      delete m_rhs_vector[n];
   }

}

void ElectroMagneticFields::initialize( const Real          a_cur_time,
                                        const std::string&  a_restart_file_name )
{
   const DisjointBoxLayout& grids(m_mesh.getDBL());
 
   if(a_restart_file_name.empty()) {
   
      if(!procID()) cout << "Initializing electromagnetic fields from input file ..." << endl;
     
      //
      // parse the initial profiles for the fields
      //
   
      GridFunctionFactory  gridFactory;
   
      LevelData<FArrayBox> temporaryProfile;
      temporaryProfile.define(grids,1,m_magneticField_virtual.ghostVect());
      LevelData<NodeFArrayBox> temporaryNodeProfile;
      temporaryNodeProfile.define(grids,1,m_electricField_virtual.ghostVect());
   
      // set initial magnetic field profile from ICs
      for (int dir=0; dir<SpaceDim; dir++) {
         stringstream sB; 
         sB << "IC.em_fields.magnetic." << dir; 
         ParmParse ppBfieldIC( sB.str().c_str() );
         RefCountedPtr<GridFunction> gridFunction(NULL);
         if(ppBfieldIC.contains("type")) {
            gridFunction = gridFactory.create(ppBfieldIC,m_mesh,m_verbosity);
         }
         else {
            gridFunction = gridFactory.createZero();
         }
         gridFunction->assign( m_magneticField, dir, m_mesh, a_cur_time );
         SpaceUtils::exchangeFluxBox(m_magneticField);
      }
   
      // set initial virtual magnetic field profile from ICs
      if(SpaceDim<3) {
         int numVirt = 3-SpaceDim;
         for( int comp=0; comp<numVirt; comp++) {
            int field_dir = comp + SpaceDim;
            stringstream sBv; 
            sBv << "IC.em_fields.magnetic." << field_dir; 
            ParmParse ppBvfieldIC( sBv.str().c_str() );
            RefCountedPtr<GridFunction> gridFunction(NULL);
            if(ppBvfieldIC.contains("type")) {
               gridFunction = gridFactory.create(ppBvfieldIC,m_mesh,m_verbosity);
            }
            else {
               gridFunction = gridFactory.createZero();
            }
            gridFunction->assign( temporaryProfile, m_mesh, a_cur_time );
            for(DataIterator dit(grids); dit.ok(); ++dit) {
               m_magneticField_virtual[dit].copy(temporaryProfile[dit],0,comp,1);
            }
            m_magneticField_virtual.exchange();      
         }
      }
   
      // set initial electric field profile from ICs
      for (int dir=0; dir<SpaceDim; dir++) {
         stringstream sE; 
         sE << "IC.em_fields.electric." << dir; 
         ParmParse ppEfieldIC( sE.str().c_str() );
         RefCountedPtr<GridFunction> gridFunction(NULL);
         if(ppEfieldIC.contains("type")) {
            gridFunction = gridFactory.create(ppEfieldIC,m_mesh,m_verbosity);
         }
         else {
            gridFunction = gridFactory.createZero();
         }
         gridFunction->assign( m_electricField, dir, m_mesh, a_cur_time );
         SpaceUtils::exchangeEdgeDataBox(m_electricField);
      }
   
      // set initial virtual electric field profile from ICs
      if(SpaceDim<3) {
         int numVirt = 3-SpaceDim;
         for( int comp=0; comp<numVirt; comp++) {
            int field_dir = comp + SpaceDim;
            stringstream sEv; 
            sEv << "IC.em_fields.electric." << field_dir; 
            ParmParse ppEvfieldIC( sEv.str().c_str() );
            RefCountedPtr<GridFunction> gridFunction(NULL);
            if(ppEvfieldIC.contains("type")) {
               gridFunction = gridFactory.create(ppEvfieldIC,m_mesh,m_verbosity);
            }
            else {
               gridFunction = gridFactory.createZero();
            }
            gridFunction->assign( temporaryNodeProfile, m_mesh, a_cur_time );
            for(DataIterator dit(grids); dit.ok(); ++dit) {
               FArrayBox& this_Ev  = m_electricField_virtual[dit].getFab();
               this_Ev.copy(temporaryNodeProfile[dit].getFab(),0,comp,1);
            }
         }
         SpaceUtils::exchangeNodeFArrayBox(m_electricField_virtual);
      }

      // apply BCs to initial profiles
      applyBCs_electricField(a_cur_time);
      applyBCs_magneticField(a_cur_time);
      
      if(m_enforce_gauss_startup) enforceGaussLaw( a_cur_time );
   
      // initialize old values by copy
      for(DataIterator dit(grids); dit.ok(); ++dit) {

         for (int dir=0; dir<SpaceDim; dir++) {
            FArrayBox& Edir_old = m_electricField_old[dit][dir];
            const Box& edge_box = Edir_old.box();
            Edir_old.copy(m_electricField[dit][dir],edge_box);
            // 
            FArrayBox& Bdir_old = m_magneticField_old[dit][dir];
            const Box& face_box = Bdir_old.box();
            Bdir_old.copy(m_magneticField[dit][dir],face_box);
         }   
      
         if(SpaceDim<3) {
            FArrayBox& Ev_old = m_electricField_virtual_old[dit].getFab();
            const Box& node_box = Ev_old.box();
            Ev_old.copy(m_electricField_virtual[dit].getFab(),node_box);
            //
            FArrayBox& Bv_old = m_magneticField_virtual_old[dit];
            const Box& cell_box = Bv_old.box();
            Bv_old.copy(m_magneticField_virtual[dit],cell_box);
         }

      }
 
      for (int dir=0; dir<SpaceDim; dir++) {
         m_intSdAdt_lo[dir] = 0.0;
         m_intSdAdt_hi[dir] = 0.0;
      }
   
   }   
   else {
   
      if(!procID()) cout << "Initializing electromagnetic fields from restart file..." << endl;

#ifdef CH_USE_HDF5
      HDF5Handle handle( a_restart_file_name, HDF5Handle::OPEN_RDONLY );
   
      //
      //  read cummulative boundary diagnostic data
      //
      HDF5HeaderData header;
      header.readFromFile( handle );

      m_intSdAdt_lo[0] = header.m_real["intSdAdt_lo0"];
      m_intSdAdt_hi[0] = header.m_real["intSdAdt_hi0"];
      if(SpaceDim>1) {
         m_intSdAdt_lo[1] = header.m_real["intSdAdt_lo1"];
         m_intSdAdt_hi[1] = header.m_real["intSdAdt_hi1"];
      }
      if(SpaceDim==3) {
         m_intSdAdt_lo[2] = header.m_real["intSdAdt_lo2"];
         m_intSdAdt_hi[2] = header.m_real["intSdAdt_hi2"];
      }
       
      // read in the magnetic field data
      handle.setGroup("magnetic_field");
      read( handle, m_magneticField, "data", grids );
   
      // read in the electric field data
      handle.setGroup("electric_field");
      read(handle, m_electricField, "data", grids);
   
#if CH_SPACEDIM<3
      // read in the virtual field data
      handle.setGroup("virtual_magnetic_field");
      read(handle, m_magneticField_virtual, "data", grids);
      
      handle.setGroup("virtual_electric_field");
      read(handle, m_electricField_virtual, "data", grids);
#endif

      //
      //  read in the old field data if it exist
      //  This data is needed for proper restart of time advance
      //  schemes that use staggering
      //

      // read in the old magnetic field data
      int group_id = handle.setGroup("magnetic_field_old");
      if(group_id<0) { // group not found, initialize by copy
         for(DataIterator dit(grids); dit.ok(); ++dit) {
            for (int dir=0; dir<SpaceDim; dir++) {
               FArrayBox& Bdir_old = m_magneticField_old[dit][dir];
               const Box& face_box = Bdir_old.box();
               Bdir_old.copy(m_magneticField[dit][dir],face_box);
            }
         }   
      }
      else read(handle, m_magneticField_old, "data", grids);

      // read in the old electric field data
      group_id = handle.setGroup("electric_field_old");
      if(group_id<0) { // group not found, initialize by copy
         for(DataIterator dit(grids); dit.ok(); ++dit) {
            for (int dir=0; dir<SpaceDim; dir++) {
               FArrayBox& Edir_old = m_electricField_old[dit][dir];
               const Box& edge_box = Edir_old.box();
               Edir_old.copy(m_electricField[dit][dir],edge_box);
            }
         }   
      }
      else read(handle, m_electricField_old, "data", grids);
   
#if CH_SPACEDIM<3
      // read in the old virtual field data
      group_id = handle.setGroup("virtual_magnetic_field_old");
      if(group_id<0) { // group not found, initialize by copy   
         for(DataIterator dit(grids); dit.ok(); ++dit) {
            FArrayBox& Bv_old = m_magneticField_virtual_old[dit];
            const Box& cell_box = Bv_old.box();
            Bv_old.copy(m_magneticField_virtual[dit],cell_box);
         }
      }
      else read(handle, m_magneticField_virtual_old, "data", grids);
 
      group_id = handle.setGroup("virtual_electric_field_old");
      if(group_id<0) { // group not found, initialize by copy   
         for(DataIterator dit(grids); dit.ok(); ++dit) {
            FArrayBox& Ev_old = m_electricField_virtual_old[dit].getFab();
            const Box& node_box = Ev_old.box();
            Ev_old.copy(m_electricField_virtual[dit].getFab(),node_box);
         }
      }
      else read(handle, m_electricField_virtual_old, "data", grids);
#endif     
 
      handle.close();

#else
      MayDay::Error("restart only defined with hdf5");
#endif
 
   }

   // set the initial divs
   setDivE();
   setDivB();

   // set the initial curls
   setCurlE();
   setCurlB();

   if(!procID()) cout << "Finished initializing electromagnetic fields " << endl << endl;

}
  
void ElectroMagneticFields::initializeMassMatricesForPC( const IntVect&  a_ncomp_xx,
		                                         const IntVect&  a_ncomp_xy,
		                                         const IntVect&  a_ncomp_xz,
		                                         const IntVect&  a_ncomp_yx,
		                                         const IntVect&  a_ncomp_yy,
		                                         const IntVect&  a_ncomp_yz,
		                                         const IntVect&  a_ncomp_zx,
		                                         const IntVect&  a_ncomp_zy,
		                                         const IntVect&  a_ncomp_zz )
{
   CH_TIME("ElectroMagneticFields::initializeMassMatricesForPC()");

   //////////////////////////////////////////////////////////////////
   //
   // This is called from PicSpeciesInterface::initializeMassMatrices()
   // The passed number of components are those corresponding to the full
   // mass matrices used to compute J. They are only used here to ensure
   // that the mass matrices used for the PC do not have more componets
   // than the mass matrices used to compute J
   //
   //////////////////////////////////////////////////////////////////
   
   if(!procID()) cout << "Initializing mass matrices for PC..." << endl;
  
   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const int ghosts(m_mesh.ghosts());
   const IntVect ghostVect = ghosts*IntVect::Unit; 

   int ncomp_xx;
   if(m_use_filtering) ncomp_xx = 1 + 2*(m_pc_mass_matrix_width+2);
   else ncomp_xx = 1 + 2*m_pc_mass_matrix_width;
   for(int dir=0; dir<SpaceDim; dir++) { 
      m_ncomp_xx_pc[dir] = std::min(ncomp_xx,a_ncomp_xx[dir]);
   }
   m_sigma_xx_pc.define(grids,m_ncomp_xx_pc.product(),ghostVect);
   SpaceUtils::zero( m_sigma_xx_pc );

   if(m_pc_mass_matrix_include_ij) {
     int ncomp_xy;
     if(m_use_filtering) ncomp_xy = 2*(m_pc_mass_matrix_width+2);
     else ncomp_xy = 2*m_pc_mass_matrix_width;
     m_ncomp_xy_pc[0] = ncomp_xy;
#if CH_SPACEDIM==2
     m_ncomp_xy_pc[1] = ncomp_xy;
#elif CH_SPACEDIM==3
     m_ncomp_xy_pc[2] = ncomp_xy + 1;
#endif
     for (int dir=0; dir<SpaceDim; dir++) {
       if(m_ncomp_xy_pc[dir]>=a_ncomp_xy[dir]) { 
         m_ncomp_xy_pc = a_ncomp_xy;
         break;
       }
     }

     int ncomp_xz;
     if(m_use_filtering) ncomp_xz = 2*(m_pc_mass_matrix_width+2);
     else ncomp_xz = 2*m_pc_mass_matrix_width;
     m_ncomp_xz_pc[0] = ncomp_xz;
#if CH_SPACEDIM==2
     m_ncomp_xz_pc[1] = ncomp_xz + 1;
#elif CH_SPACEDIM==3
     m_ncomp_xz_pc[2] = ncomp_xz ;
#endif
     for (int dir=0; dir<SpaceDim; dir++) {
       if(m_ncomp_xz_pc[dir]>=a_ncomp_xz[dir]) { 
         m_ncomp_xz_pc = a_ncomp_xz;
         break;
       }
     }
     m_sigma_xy_pc.define(grids,m_ncomp_xy_pc.product(),ghostVect);
     m_sigma_xz_pc.define(grids,m_ncomp_xz_pc.product(),ghostVect);
     SpaceUtils::zero( m_sigma_xy_pc );
     SpaceUtils::zero( m_sigma_xz_pc );
   }
   else{
     m_ncomp_xy_pc = IntVect::Zero;
     m_ncomp_xz_pc = IntVect::Zero;
   }
   
#if CH_SPACEDIM==1
   // define the PC mass matrices corresponding to Jy
   m_ncomp_yy_pc[0] = 1 + 2*m_pc_mass_matrix_width;
   if(m_ncomp_yy_pc[0]>a_ncomp_yy[0]) {m_ncomp_yy_pc[0] = a_ncomp_yy[0];}
   m_sigma_yy_pc.define(grids,m_ncomp_yy_pc[0],ghostVect);
   SpaceUtils::zero( m_sigma_yy_pc );

   if(m_pc_mass_matrix_include_ij) {
     m_ncomp_yx_pc[0] = 2*m_pc_mass_matrix_width;
     m_ncomp_yz_pc[0] = 1 + 2*m_pc_mass_matrix_width;
     if(m_ncomp_yx_pc[0]>a_ncomp_yx[0]) {m_ncomp_yx_pc = a_ncomp_yx;}
     if(m_ncomp_yz_pc[0]>a_ncomp_yz[0]) {m_ncomp_yz_pc = a_ncomp_yz;}
     m_sigma_yx_pc.define(grids,m_ncomp_yx_pc[0],ghostVect);
     m_sigma_yz_pc.define(grids,m_ncomp_yz_pc[0],ghostVect);
     SpaceUtils::zero( m_sigma_yx_pc );
     SpaceUtils::zero( m_sigma_yz_pc );
   }
   else{
     m_ncomp_yx_pc = IntVect::Zero;
     m_ncomp_yz_pc = IntVect::Zero;
   }
#elif CH_SPACEDIM==2
   int ncomp_yy = 1 + 2*m_pc_mass_matrix_width;
   for(int dir=0; dir<SpaceDim; dir++) { 
      m_ncomp_yy_pc[dir] = std::min(ncomp_yy,a_ncomp_yy[dir]);
   }
   
   if(m_pc_mass_matrix_include_ij) {
     m_ncomp_yx_pc[0] = 2*m_pc_mass_matrix_width;
     m_ncomp_yx_pc[1] = 2*m_pc_mass_matrix_width;
     m_ncomp_yz_pc[0] = 1 + 2*m_pc_mass_matrix_width;
     m_ncomp_yz_pc[1] = 2*m_pc_mass_matrix_width;
     for (int dir=0; dir<SpaceDim; dir++) {
       if(m_ncomp_yx_pc[dir]>=a_ncomp_yx[dir]) { 
         m_ncomp_yx_pc = a_ncomp_yx;
         break;
       }
     }
     for (int dir=0; dir<SpaceDim; dir++) {
       if(m_ncomp_yz_pc[dir]>=a_ncomp_yz[dir]) { 
         m_ncomp_yz_pc = a_ncomp_yz;
         break;
       }
     }
   }
   else{
     m_ncomp_yx_pc = IntVect::Zero;
     m_ncomp_yz_pc = IntVect::Zero;
   }
#endif

   // define the PC mass matrices corresponding to Jz
   int ncomp_zz = 1 + 2*m_pc_mass_matrix_width;
   for(int dir=0; dir<SpaceDim; dir++) { m_ncomp_zz_pc[dir] = ncomp_zz; }
   if(m_ncomp_zz_pc[0]>a_ncomp_zz[0]) {m_ncomp_zz_pc = a_ncomp_zz;}
   m_sigma_zz_pc.define(grids,m_ncomp_zz_pc.product(),ghostVect);
   SpaceUtils::zero( m_sigma_zz_pc );

   if(m_pc_mass_matrix_include_ij) {
     m_ncomp_zx_pc[0] = 2*m_pc_mass_matrix_width;
     m_ncomp_zy_pc[0] = 1+2*m_pc_mass_matrix_width;
#if CH_SPACEDIM==2
      m_ncomp_zx_pc[1] = 1+2*m_pc_mass_matrix_width;
     m_ncomp_zy_pc[1] = 2*m_pc_mass_matrix_width;
#elif CH_SPACEDIM==3
     m_ncomp_zx_pc[2] = 2*m_pc_mass_matrix_width;
     m_ncomp_zy_pc[2] = 2*m_pc_mass_matrix_width;
#endif
     for (int dir=0; dir<SpaceDim; dir++) {
       if(m_ncomp_zx_pc[dir]>=a_ncomp_zx[dir]) { 
         m_ncomp_zx_pc = a_ncomp_zx;
         break;
       }
     }
     for (int dir=0; dir<SpaceDim; dir++) {
       if(m_ncomp_zy_pc[dir]>=a_ncomp_zy[dir]) { 
         m_ncomp_zy_pc = a_ncomp_zy;
         break;
       }
     }
     m_sigma_zx_pc.define(grids,m_ncomp_zx_pc.product(),ghostVect);
     m_sigma_zy_pc.define(grids,m_ncomp_zy_pc.product(),ghostVect);
     SpaceUtils::zero( m_sigma_zx_pc );
     SpaceUtils::zero( m_sigma_zy_pc );
   }
   else{
     m_ncomp_zx_pc = IntVect::Zero;
     m_ncomp_zy_pc = IntVect::Zero;
   }
   
   if(!procID()) {
      if(m_use_filtering) cout << " filtering is being used ... " << endl;
      cout << " pc_mass_matrix_width = " << m_pc_mass_matrix_width << endl;
      cout << " pc_mass_matrix_include_ij = " << m_pc_mass_matrix_include_ij << endl;
#if CH_SPACEDIM>1
      if(m_pc_mass_matrix_width>0) {
	 cout << " WARNING: only diagonal elements (width = 0) are included in the PC for 2D thus far" << endl;
      }
      if(m_pc_mass_matrix_include_ij) {
	 cout << " WARNING: ij cross terms are not included in the PC for 2D thus far" << endl;
      }
#endif
      cout << " ncomp_xx_pc = " << m_ncomp_xx_pc << endl;
      cout << " ncomp_xy_pc = " << m_ncomp_xy_pc << endl;
      cout << " ncomp_xz_pc = " << m_ncomp_xz_pc << endl;
      cout << " ncomp_yx_pc = " << m_ncomp_yx_pc << endl;
      cout << " ncomp_yy_pc = " << m_ncomp_yy_pc << endl;
      cout << " ncomp_yz_pc = " << m_ncomp_yz_pc << endl;
      cout << " ncomp_zx_pc = " << m_ncomp_zx_pc << endl;
      cout << " ncomp_zy_pc = " << m_ncomp_zy_pc << endl;
      cout << " ncomp_zz_pc = " << m_ncomp_zz_pc << endl;
      cout << "Finished initializing mass matrices for PC " << endl << endl;
   }

}


void ElectroMagneticFields::initExternalFields()
{

   // define grid function describing external fields
   
   GridFunctionFactory  gridFactory;
   
   //
   //  define external electric field profiles
   //

   stringstream sE0;
   sE0 << "em_fields.external.electric.0";
   ParmParse ppExtE0( sE0.str().c_str() );
   m_gridFunction_extE0 = gridFactory.create(ppExtE0,m_mesh,m_verbosity);
   
   stringstream sE1;
   sE1 << "em_fields.external.electric.1";
   ParmParse ppExtE1( sE1.str().c_str() );
   m_gridFunction_extE1 = gridFactory.create(ppExtE1,m_mesh,m_verbosity);
   
   stringstream sE2;
   sE2 << "em_fields.external.electric.2";
   ParmParse ppExtE2( sE2.str().c_str() );
   m_gridFunction_extE2 = gridFactory.create(ppExtE2,m_mesh,m_verbosity);
   
   //
   //  define external magnetic field profiles
   //
   
   stringstream sB0;
   sB0 << "em_fields.external.magnetic.0";
   ParmParse ppExtB0( sB0.str().c_str() );
   m_gridFunction_extB0 = gridFactory.create(ppExtB0,m_mesh,m_verbosity);
   
   stringstream sB1;
   sB1 << "em_fields.external.magnetic.1";
   ParmParse ppExtB1( sB1.str().c_str() );
   m_gridFunction_extB1 = gridFactory.create(ppExtB1,m_mesh,m_verbosity);
   
   stringstream sB2;
   sB2 << "em_fields.external.magnetic.2";
   ParmParse ppExtB2( sB2.str().c_str() );
   m_gridFunction_extB2 = gridFactory.create(ppExtB2,m_mesh,m_verbosity);

}

void ElectroMagneticFields::computeRHSMagneticField(  const Real a_time,
                                                      const Real a_cnormDt )
{
  CH_TIME("ElectroMagneticFields::computeRHSMagneticField()");
  
  setCurlE();

  if (!advanceB()) return;

  auto grids(m_mesh.getDBL());

  if(m_advanceB_inPlane) {
    SpaceUtils::zero( m_magneticField_rhs );
    for(auto dit(grids.dataIterator()); dit.ok(); ++dit) {
      for (int dir=0; dir < SpaceDim; dir++) {
        if(m_advanceB_comp[dir]) {
           FArrayBox& Brhsdir = m_magneticField_rhs[dit][dir];
           Brhsdir.copy( m_curlE[dit][dir] );
           Brhsdir.mult(-a_cnormDt);
        }
      }
    }
  }

#if CH_SPACEDIM<3
  if(m_advanceB_virtual) {
    SpaceUtils::zero( m_magneticField_virtual_rhs );
    for(auto dit(grids.dataIterator()); dit.ok(); ++dit) {
      Box cell_box = grids[dit];
      const int Ncomp = m_magneticField_virtual.nComp();
      for (int comp=0; comp<Ncomp; comp++){
        FArrayBox& Bvrhsdir  = m_magneticField_virtual_rhs[dit];
        Bvrhsdir.copy(m_curlE_virtual[dit], comp, comp);
        Bvrhsdir.mult(-a_cnormDt*m_advanceB_comp[SpaceDim+comp], comp);
      }
    }
  }
#endif

  return;
}

void ElectroMagneticFields::computeRHSElectricField(  const Real a_time, 
                                                      const Real a_cnormDt )
{
  CH_TIME("ElectroMagneticFields::computeRHSElectricField()");
    
  setCurlB();
  if(m_use_poisson) enforceGaussLaw( a_time );

  if(!advanceE()) return;

  auto grids(m_mesh.getDBL());

  if(m_advanceE_inPlane) {
    SpaceUtils::zero( m_electricField_rhs );
    for(auto dit(grids.dataIterator()); dit.ok(); ++dit) {
      for (int dir=0; dir<SpaceDim; dir++) {
        if(m_advanceE_comp[dir]) {
          const FArrayBox& curlBdir = m_curlB[dit][dir];
          const FArrayBox& Jdir = m_currentDensity[dit][dir];
          FArrayBox& Erhsdir = m_electricField_rhs[dit][dir];

          Erhsdir.copy( Jdir );
          Erhsdir.mult( -m_Jnorm_factor );
          Erhsdir.plus( curlBdir );
          Erhsdir.mult( a_cnormDt );
        }
      }
    }
  }

#if CH_SPACEDIM<3
  if(m_advanceE_virtual) {
    SpaceUtils::zero( m_electricField_virtual_rhs );
    for(auto dit(grids.dataIterator()); dit.ok(); ++dit) {
      const int Ncomp = m_electricField_virtual.nComp();
      for (int comp=0; comp<Ncomp; comp++){
        if(m_advanceE_comp[SpaceDim+comp]) {
          const FArrayBox& curlBv = m_curlB_virtual[dit].getFab();
          const FArrayBox& Jv = m_currentDensity_virtual[dit].getFab();
          FArrayBox& Evrhsdir = m_electricField_virtual_rhs[dit].getFab();

          Evrhsdir.copy( Jv, comp, comp );
          Evrhsdir.mult( -m_Jnorm_factor, comp );
          Evrhsdir.plus( curlBv, comp, comp );
          Evrhsdir.mult( a_cnormDt, comp );
        }
      }
    }
  }
#endif

  return;
}

void ElectroMagneticFields::applyBCs_electricField( const Real  a_time )
{
   CH_TIME("ElectroMagneticFields::applyBCs_electricField()");
   
   if(m_advanceE_inPlane) {
     
      // exchange to fill internal ghost cells 
      //SpaceUtils::exchangeEdgeDataBox(m_electricField);
     
      // apply the boundary bcs 
      m_field_bc->applyEdgeBC( m_electricField, a_time );

   }
   
   if(SpaceDim<3 && m_advanceE_virtual) {
      
      // exchange to fill internal ghost cells 
      //SpaceUtils::exchangeNodeFArrayBox(m_electricField_virtual);
      
      // apply the boundary bcs 
      m_field_bc->applyNodeBC( m_electricField_virtual, a_time );
   
   }

}

void ElectroMagneticFields::applyBCs_magneticField( const Real  a_time )
{
   CH_TIME("ElectroMagneticFields::applyBCs_magneticField()");

   if(m_advanceB_inPlane) {
      
      // exchange to fill internal ghost cells 
      //SpaceUtils::exchangeFluxBox(m_magneticField);
      
      // apply the boundary bcs 
      m_field_bc->applyFluxBC( m_magneticField, a_time );

   }
 
   if(SpaceDim<3 && m_advanceB_virtual) {
         
      // exchange to fill internal ghost cells 
      //m_magneticField_virtual.exchange();
      
      // apply the boundary bcs 
      m_field_bc->applyCellBC( m_magneticField_virtual, a_time );

   }

}
  
void ElectroMagneticFields::postTimeStep( const Real  a_dt )
{
   CH_TIME("ElectroMagneticFields::postTimeStep()");
 
   if(m_field_bc==NULL) return;
 
   // update surface integrals of poynting flux for diagnostics
   // by default, value for periodic BCs will be zero 

   RealVect intSdA_lo_local(D_DECL6(0,0,0,0,0,0));
   RealVect intSdA_hi_local(D_DECL6(0,0,0,0,0,0));
   m_field_bc->computeIntSdA( intSdA_lo_local, intSdA_hi_local,
                              m_electricField,
                              m_magneticField,
                              m_electricField_virtual,
                              m_magneticField_virtual );
  
   intSdA_lo_local = intSdA_lo_local*m_SdA_factor; // [Joules/s]
   intSdA_hi_local = intSdA_hi_local*m_SdA_factor; // [Joules/s]
   m_intSdA_lo = intSdA_lo_local;
   m_intSdA_hi = intSdA_hi_local;
#ifdef CH_MPI
   MPI_Allreduce( &intSdA_lo_local,
                  &m_intSdA_lo,
                  SpaceDim,
                  MPI_CH_REAL,
                  MPI_SUM,
                  MPI_COMM_WORLD );
   MPI_Allreduce( &intSdA_hi_local,
                  &m_intSdA_hi,
                  SpaceDim,
                  MPI_CH_REAL,
                  MPI_SUM,
                  MPI_COMM_WORLD );
#endif

   // update running integral over time
   for (int dir=0; dir<SpaceDim; ++dir) {
      m_intSdAdt_lo[dir] += m_intSdA_lo[dir]*a_dt*m_time_scale; // [Joules]
      m_intSdAdt_hi[dir] += m_intSdA_hi[dir]*a_dt*m_time_scale; // [Joules]
   }

}

void ElectroMagneticFields::setPoyntingFlux() 
{
   CH_TIME("ElectroMagneticFields::setPoyntingFlux()");
   
   const DisjointBoxLayout& grids(m_mesh.getDBL());
  
   if(SpaceDim==3) {
  
      // worry about it later

   }
   
   if(SpaceDim==2) {

      for(DataIterator dit(grids); dit.ok(); ++dit) {
   
               FArrayBox& ExB_0 = m_ExB[dit][0];
               FArrayBox& ExB_1 = m_ExB[dit][1];
               FArrayBox& ExB_v = m_EvxB[dit].getFab();
         const FArrayBox& E0 = m_electricField[dit][0];           
         const FArrayBox& E1 = m_electricField[dit][1];           
         const FArrayBox& Ev = m_electricField_virtual[dit].getFab();           
         const FArrayBox& B0 = m_magneticField[dit][0];           
         const FArrayBox& B1 = m_magneticField[dit][1];           
         const FArrayBox& Bv = m_magneticField_virtual[dit];        
   
         Box node_box = surroundingNodes(grids[dit]);
         Box edge_box_0 = enclosedCells(node_box,0);
         Box edge_box_1 = enclosedCells(node_box,1);
         
         // ExB_0(0) = Ex*By (or -Ex*Bz)
         ExB_0.copy(E0,edge_box_0,0,edge_box_0,0,1);
         ExB_0.mult(B1,edge_box_0,0,0,1);
	 if(m_mesh.anticyclic()) ExB_0.negate(0,1);
         
         // ExB_0(1) = -Ex*Bz (or Ex*By)
         SpaceUtils::interpolateStag(ExB_0,edge_box_0,1,Bv,0,1);
         ExB_0.mult(E0,edge_box_0,0,1,1);
	 if(!m_mesh.anticyclic()) ExB_0.negate(1,1);
         
         // ExB_1(0) = Ey*Bz (or -Ez*By)
         SpaceUtils::interpolateStag(ExB_1,edge_box_1,0,Bv,0,0);
         ExB_1.mult(E1,edge_box_1,0,0,1);
	 if(m_mesh.anticyclic()) ExB_1.negate(0,1);
         
         // ExB_1(1) = -Ey*Bx (or Ez*Bx)
         ExB_1.copy(B0,edge_box_1,0,edge_box_1,1,1);
         ExB_1.mult(E1,edge_box_1,0,1,1);
	 if(!m_mesh.anticyclic()) ExB_1.negate(1,1);
         
         // ExB_v(0) = Ez*Bx (or -Ey*Bx)
         SpaceUtils::interpolateStag(ExB_v,node_box,0,B0,0,1);
         ExB_v.mult(Ev,node_box,0,0,1);
         if(m_mesh.anticyclic()) ExB_v.negate(0,1);

         // ExB_v(1) = -Ez*By (or Ey*Bz)
         SpaceUtils::interpolateStag(ExB_v,node_box,1,B1,0,0);
         ExB_v.mult(Ev,node_box,0,1,1);
         if(!m_mesh.anticyclic()) ExB_v.negate(1,1);
    
      }

   }
   
   if(SpaceDim==1) {

      for(DataIterator dit(grids); dit.ok(); ++dit) {
   
               FArrayBox& ExB = m_ExB[dit][0];
               FArrayBox& ExB_v = m_EvxB[dit].getFab();
         const FArrayBox& E0 = m_electricField[dit][0];           
         const FArrayBox& Ev = m_electricField_virtual[dit].getFab();           
         const FArrayBox& B0 = m_magneticField[dit][0];           
         const FArrayBox& Bv = m_magneticField_virtual[dit];        
   
         Box cell_box = grids[dit];
         Box node_box = surroundingNodes(grids[dit]);
         
         // ExB(0) = Ex*By
         ExB.copy(E0,cell_box,0,cell_box,0,1);
         ExB.mult(Bv,cell_box,0,0,1);
         
         // ExB(1) = -Ex*Bz
         ExB.copy(E0,cell_box,0,cell_box,1,1);
         ExB.mult(Bv,cell_box,1,1,1);
         ExB.negate(1,1);
         
         // ExB_v(0) = Ey*Bz
         SpaceUtils::interpolateStag(ExB_v,node_box,0,Bv,1,0);
         ExB_v.mult(Ev,node_box,0,0,1);
         
         // ExB_v(1) = -Ey*Bx
         ExB_v.copy(Ev,node_box,0,node_box,1,1);
         ExB_v.mult(B0,node_box,0,1,1);
         ExB_v.negate(1,1);
         
         // ExB_v(2) = Ez*Bx
         ExB_v.copy(Ev,node_box,1,node_box,2,1);
         ExB_v.mult(B0,node_box,0,2,1);
         
         // ExB_v(3) = -Ez*By
         SpaceUtils::interpolateStag(ExB_v,node_box,3,Bv,0,0);
         ExB_v.mult(Ev,node_box,1,3,1);
         ExB_v.negate(3,1);
     
      }

   }  

}

Real ElectroMagneticFields::electricFieldEnergy() const
{
  CH_TIME("ElectroMagneticFields::electricFieldEnergy()");
  const DisjointBoxLayout& grids(m_mesh.getDBL());
 
  Real energyE_local = 0.0;

  const LevelData<EdgeDataBox>& masked_Ja_ec(m_mesh.getMaskedJec());
  for(DataIterator dit(grids); dit.ok(); ++dit) {
    for(int dir=0; dir<SpaceDim; dir++) {
      Box edge_box = grids[dit];
      edge_box.surroundingNodes();
      edge_box.enclosedCells(dir);
      
      const FArrayBox& this_E  = m_electricField[dit][dir];
      const FArrayBox& this_Ja = masked_Ja_ec[dit][dir];
      for (BoxIterator bit(edge_box); bit.ok(); ++bit) {
        energyE_local += this_E(bit(),0)*this_E(bit(),0)*this_Ja(bit(),0);
      }
    }
  }
  
#if CH_SPACEDIM<3
  const LevelData<NodeFArrayBox>& masked_Ja_nc(m_mesh.getMaskedJnc());
  for(DataIterator dit(grids); dit.ok(); ++dit) {
    Box node_box = grids[dit];
    node_box.surroundingNodes();

    const FArrayBox& this_E  = m_electricField_virtual[dit].getFab();
    const FArrayBox& this_Ja = masked_Ja_nc[dit].getFab();
    for(int n=0; n<this_E.nComp(); n++) {
      for (BoxIterator bit(node_box); bit.ok(); ++bit) {
        energyE_local += this_E(bit(),n)*this_E(bit(),n)*this_Ja(bit(),0);
      }
    }
  }
#endif
  
  energyE_local = energyE_local*m_energyE_factor; // [Joules]
  
  Real energyE_global = 0.0;
#ifdef CH_MPI
  MPI_Allreduce(  &energyE_local,
                  &energyE_global,
                  1,
                  MPI_DOUBLE,
                  MPI_SUM,
                  MPI_COMM_WORLD );
#else
  energyE_global = energyE_local;
#endif

  return energyE_global;
}

Real ElectroMagneticFields::magneticFieldEnergy() const
{
  CH_TIME("ElectroMagneticFields::magneticFieldEnergy()");
  const DisjointBoxLayout& grids(m_mesh.getDBL());
 
  Real energyB_local = 0.0;
  
  const LevelData<FluxBox>& masked_Ja_fc(m_mesh.getMaskedJfc());
  for(DataIterator dit(grids); dit.ok(); ++dit) {
    for(int dir=0; dir<SpaceDim; dir++) {
      Box face_box = grids[dit];
      face_box.surroundingNodes(dir);
      
      const FArrayBox& this_B  = m_magneticField[dit][dir];
      const FArrayBox& this_Ja = masked_Ja_fc[dit][dir];
      for (BoxIterator bit(face_box); bit.ok(); ++bit) {
        energyB_local += this_B(bit(),0)*this_B(bit(),0)*this_Ja(bit(),0);
      }
    }
  }
  
#if CH_SPACEDIM<3
  const LevelData<FArrayBox>& Ja_cc(m_mesh.getJcc());
  for(DataIterator dit(grids); dit.ok(); ++dit) {
    Box cell_box = grids[dit];

    const FArrayBox& this_B  = m_magneticField_virtual[dit];
    const FArrayBox& this_Ja = Ja_cc[dit];
    for(int n=0; n<this_B.nComp(); n++) {
      for (BoxIterator bit(cell_box); bit.ok(); ++bit) {
        energyB_local += this_B(bit(),n)*this_B(bit(),n)*this_Ja(bit(),0);
      }
    }
  }
#endif
   
  energyB_local = energyB_local*m_energyB_factor; // [Joules]

  Real energyB_global = 0.0;
#ifdef CH_MPI
  MPI_Allreduce(  &energyB_local,
                  &energyB_global,
                  1,
                  MPI_CH_REAL,
                  MPI_SUM,
                  MPI_COMM_WORLD );
#else
  energyB_global = energyB_local;
#endif

  return energyB_global;

}

Real ElectroMagneticFields::max_wc0dt( const CodeUnits&  a_units,
                                       const Real&       a_dt ) const
{
   Real max_absB_local = 0.0;

   const DisjointBoxLayout& grids(m_mesh.getDBL());
   for(DataIterator dit(grids); dit.ok(); ++dit) {
      for(int dir=0; dir<SpaceDim; dir++) {
         Box face_box = grids[dit];
         face_box.surroundingNodes(dir);
         Real box_max = m_magneticField[dit][dir].max(face_box,0);
         Real box_min = m_magneticField[dit][dir].min(face_box,0);
         box_max = Max(box_max,abs(box_min));
         max_absB_local = Max(max_absB_local,box_max);
      }
      if(SpaceDim<3) {
         Box cell_box = grids[dit];
         for(int n=0; n<m_magneticField_virtual.nComp(); n++) {
            Real box_max = m_magneticField_virtual[dit].max(cell_box,n);
            Real box_min = m_magneticField_virtual[dit].min(cell_box,n);
            box_max = Max(box_max,abs(box_min));
            max_absB_local = Max(max_absB_local,box_max);
         }
      }
   } 

   Real max_absB_global = max_absB_local;
#ifdef CH_MPI
   MPI_Allreduce( &max_absB_local,
                  &max_absB_global,
                  1,
                  MPI_CH_REAL,
                  MPI_MAX,
                  MPI_COMM_WORLD );
#endif
 
  const Real dt_sec = a_dt*a_units.getScale(a_units.TIME);
  const Real B_T = max_absB_global*a_units.getScale(a_units.MAGNETIC_FIELD);
  
  const Real max_wc0dt = B_T*Constants::QE/Constants::ME*dt_sec; 
  return max_wc0dt;

}

void ElectroMagneticFields::setCurlB()
{
   CH_TIME("ElectroMagneticFields::setCurlB()");
    
   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const RealVect& dX(m_mesh.getdX());
  
#if CH_SPACEDIM==3

   for(DataIterator dit(grids); dit.ok(); ++dit) {

      for (int dir=0; dir<SpaceDim; dir++) {
           
         int dirj = dir + 1;
         dirj = dirj % SpaceDim;
         int dirk = dir + 2;
         dirk = dirk % SpaceDim;

         FArrayBox& curlB_dir = m_curlB[dit][dir];
         const FArrayBox& B_dirj = m_magneticField[dit][dirj];           
         const FArrayBox& B_dirk = m_magneticField[dit][dirk];

         int additive = 0;
         SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], 0,
                                         B_dirk, 0,
                                         dX[dirj], dirj,
                                         additive ); 
         additive = 1;
         SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], 0,
                                         B_dirj, 0,
                                         -dX[dirk], dirk,
                                         additive ); 
            
      }

   }

   SpaceUtils::exchangeEdgeDataBox(m_curlB);

#elif CH_SPACEDIM==2
      
   const LevelData<FArrayBox>& Jcc = m_mesh.getJcc();
   const LevelData<EdgeDataBox>& Jec = m_mesh.getJec();
   const bool anticyclic = m_mesh.anticyclic();

   for(DataIterator dit(grids); dit.ok(); ++dit) {
   
      // FluxBox ==> NodeFArrayBox
 
      Box node_box = grids[dit];

      int dirj = 0;
      int dirk = 1;

      FArrayBox& curlBv = m_curlB_virtual[dit].getFab();
      const FArrayBox& B_dirj = m_magneticField[dit][dirj];           
      const FArrayBox& B_dirk = m_magneticField[dit][dirk];

      int additive = 0;
      SpaceUtils::simpleStagGradComp( curlBv, grids[dit], 0,
                                      B_dirk, 0,
                                      dX[dirj], dirj,
                                      additive ); 
      additive = 1;
      SpaceUtils::simpleStagGradComp( curlBv, grids[dit], 0,
                                      B_dirj, 0,
                                      -dX[dirk], dirk,
                                      additive ); 
      if(anticyclic) curlBv.negate();
       
      // FArrayBox ==> EdgeDataBox
         
      for (int dir=0; dir<SpaceDim; dir++) {
           
         int dirj = dir + 1;
         dirj = dirj % SpaceDim;

         FArrayBox& curlB_dir = m_curlB[dit][dir];
         const FArrayBox& B_virt = m_magneticField_virtual[dit];           

         int additive = 0;
         int sign = 1-2*dir;
         if(anticyclic) sign = -sign;
         if(m_mesh.axisymmetric() && dir==1) {
            FArrayBox JaBv;
            JaBv.define(B_virt.box(),1); 
            JaBv.copy(B_virt,0,0,1);           
            JaBv.mult(Jcc[dit],0,0,1);           
            SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], 0,
                                            JaBv, 0,
                                            sign*dX[dirj], dirj,
                                            additive ); 
            curlB_dir.divide(Jec[dit][dir],0,0,1); // NEED TO APPLY ON-AXIS CORRECTION          
         }
         else {
            SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], 0,
                                            B_virt, 0,
                                            sign*dX[dirj], dirj,
                                            additive );
         }
            
      }

   }

   m_field_bc->applyOnAxisCurlBC( m_curlB, m_magneticField_virtual );
   SpaceUtils::exchangeEdgeDataBox(m_curlB);
   SpaceUtils::exchangeNodeFArrayBox(m_curlB_virtual);

#elif CH_SPACEDIM==1 
      
   const string& geom_type = m_mesh.geomType();
   const LevelData<FArrayBox>& Jcc = m_mesh.getJcc();
   const LevelData<NodeFArrayBox>& Jnc = m_mesh.getJnc();

   for(DataIterator dit(grids); dit.ok(); ++dit) {

      int dirj = 0;
      int dirk = 1;

      FArrayBox& curlB_dir = m_curlB_virtual[dit].getFab();
      const FArrayBox& B_virt = m_magneticField_virtual[dit];           

      int additive = 0;
      if(m_mesh.axisymmetric() && geom_type=="sph_R") {      
         FArrayBox JaBz(B_virt.box(),1); 
         JaBz.copy(B_virt,dirk,0,1);           
         JaBz.mult(Jcc[dit],0,0,1);           
         SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], dirj,
                                         JaBz, 0,
                                         -dX[0], 0,
                                         additive );
         curlB_dir.divide(Jnc[dit].getFab(),0,dirj,1); // NEED TO APPLY ON AXIS CORRECTION
      }
      else { 
         SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], dirj,
                                         B_virt, dirk,
                                         -dX[0], 0,
                                         additive ); 
      }

      if(m_mesh.axisymmetric()) {      
         FArrayBox JaBy(B_virt.box(),1); 
         JaBy.copy(B_virt,dirj,0,1);           
         JaBy.mult(Jcc[dit],0,0,1);           
         SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], dirk,
                                         JaBy, 0,
                                         dX[0], 0,
                                         additive );
         curlB_dir.divide(Jnc[dit].getFab(),0,dirk,1); // NEED TO APPLY ON AXIS CORRECTION
      }
      else { 
         SpaceUtils::simpleStagGradComp( curlB_dir, grids[dit], dirk,
                                         B_virt, dirj,
                                         dX[0], 0,
                                         additive );
      }

   }
   
   m_field_bc->applyOnAxisCurlBC( m_curlB_virtual, m_magneticField_virtual );
   SpaceUtils::exchangeEdgeDataBox(m_curlB);
   SpaceUtils::exchangeNodeFArrayBox(m_curlB_virtual);
   
#endif 
  
}

void ElectroMagneticFields::setCurlE()
{
   CH_TIME("ElectroMagneticFields::setCurlE()");
    
   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const RealVect& dX(m_mesh.getdX());
  
#if CH_SPACEDIM==3 // EdgeDataBox ==> FluxBox

   for(DataIterator dit(grids); dit.ok(); ++dit) {
      for (int dir=0; dir<SpaceDim; dir++) {
           
         int dirj, dirk;
         dirj = dir + 1;
         dirj = dirj % SpaceDim;
         dirk = dir + 2;
         dirk = dirk % SpaceDim;

         FArrayBox& curlE_dir = m_curlE[dit][dir];
         const FArrayBox& E_dirj = m_electricField[dit][dirj];           
         const FArrayBox& E_dirk = m_electricField[dit][dirk];

         int additive = 0;
         SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], 0,
                                         E_dirk, 0,
                                         dX[dirj], dirj,
                                         additive ); 
         additive = 1;
         SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], 0,
                                         E_dirj, 0,
                                         -dX[dirk], dirk,
                                         additive ); 
            
      }
   }
   
#elif CH_SPACEDIM==2 // EdgeDataBox ==> FArrayBox and NodeFArrayBox ==> FluxBox
      
    const bool anticyclic = m_mesh.anticyclic();
    const LevelData<NodeFArrayBox>& Jnc = m_mesh.getJnc();
    const LevelData<FluxBox>& Jfc = m_mesh.getJfc();

    for(DataIterator dit(grids); dit.ok(); ++dit) {
   
      //        
      // EdgeDataBox ==> FArrayBox
      //

      int dirj, dirk;
      dirj = 0;
      dirk = 1;

      FArrayBox& curlEv = m_curlE_virtual[dit];
      const FArrayBox& E_dirj = m_electricField[dit][dirj];           
      const FArrayBox& E_dirk = m_electricField[dit][dirk];

      int additive = 0;
      SpaceUtils::simpleStagGradComp( curlEv, grids[dit], 0,
                                      E_dirk, 0,
                                      dX[dirj], dirj,
                                      additive ); 
      additive = 1;
      SpaceUtils::simpleStagGradComp( curlEv, grids[dit], 0,
                                      E_dirj, 0,
                                      -dX[dirk], dirk,
                                      additive ); 
      if(anticyclic) curlEv.negate();

      //
      // NodeFArrayBox ==> FluxBox
      //
         
      for (int dir=0; dir<SpaceDim; dir++) {
           
         int dirj;
         dirj = dir + 1;
         dirj = dirj % SpaceDim;

         FArrayBox& curlE_dir = m_curlE[dit][dir];
         const FArrayBox& E_virt = m_electricField_virtual[dit].getFab();           

         int additive = 0;
         int sign = 1-2*dir;
         if(anticyclic) sign = -sign;
         if(m_mesh.axisymmetric() && dir==1) {
            FArrayBox JaEy;
            JaEy.define(E_virt.box(),1); 
            JaEy.copy(E_virt,0,0,1);           
            JaEy.mult(Jnc[dit].getFab(),0,0,1);           
            SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], 0,
                                            JaEy, 0,
                                            sign*dX[dirj], dirj,
                                            additive ); 
            curlE_dir.divide(Jfc[dit][dir],0,0,1);          
         }
         else {
            SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], 0,
                                            E_virt, 0,
                                            sign*dX[dirj], dirj,
                                            additive );
	 } 
      
      }

   }
   
#elif CH_SPACEDIM==1 // 2 comp NodeFArrayBox ==> 2 comp FArrayBox

   const string& geom_type = m_mesh.geomType();
   const LevelData<FArrayBox>& Jcc = m_mesh.getJcc();
   const LevelData<NodeFArrayBox>& Jnc = m_mesh.getJnc();

   for(DataIterator dit(grids); dit.ok(); ++dit) {
   
      int dirj = 0;
      int dirk = 1;

      FArrayBox& curlE_dir = m_curlE_virtual[dit];
      const FArrayBox& E_virt = m_electricField_virtual[dit].getFab();           

      int additive = 0;
      if(m_mesh.axisymmetric() && geom_type=="sph_R") {      
         FArrayBox JaEz(E_virt.box(),1); 
         JaEz.copy(E_virt,dirk,0,1);           
         JaEz.mult(Jnc[dit].getFab(),0,0,1);           
         SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], dirj,
                                         JaEz, 0,
                                         -dX[0], 0,
                                         additive );
         curlE_dir.divide(Jcc[dit],0,dirj,1);
      }
      else {
         SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], dirj,
                                         E_virt, dirk,
                                         -dX[0], 0,
                                         additive );
      } 

      if(m_mesh.axisymmetric()) {      
         FArrayBox JaEy(E_virt.box(),1); 
         JaEy.copy(E_virt,dirj,0,1);           
         JaEy.mult(Jnc[dit].getFab(),0,0,1);           
         SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], dirk,
                                         JaEy, 0,
                                         dX[0], 0,
                                         additive );
         curlE_dir.divide(Jcc[dit],0,dirk,1);
      }
      else {
         SpaceUtils::simpleStagGradComp( curlE_dir, grids[dit], dirk,
                                         E_virt, dirj,
                                         dX[0], 0,
                                         additive );
      } 

   }
  
#endif

   SpaceUtils::exchangeFluxBox(m_curlE);
   if(SpaceDim<3) m_curlE_virtual.exchange();   
   
}

void ElectroMagneticFields::setDivE()
{
   CH_TIME("ElectroMagneticFields::setDivE()");
    
   // get some Grid info
   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const LevelData<EdgeDataBox>& Jec = m_mesh.getJec();
   const LevelData<NodeFArrayBox>& Jnc = m_mesh.getJnc();
   const RealVect& dX(m_mesh.getdX());
  
   // first do dir=0 to address singularity on axis if using cyl coords
   for(DataIterator dit(grids); dit.ok(); ++dit) {
         
      FArrayBox& this_divE( m_divE[dit].getFab() );
      Box node_box = grids[dit];
      node_box.surroundingNodes();
         
      this_divE.setVal(0.0, node_box, 0, this_divE.nComp());
      int additive = 0;
      int dir = 0;
      const FArrayBox& this_E( m_electricField[dit][dir] );
      FArrayBox JaE0;
      JaE0.define(this_E.box(),1); 
      JaE0.copy(this_E,0,0,1);           
      JaE0.mult(Jec[dit][dir],0,0,1);           
      SpaceUtils::simpleStagGradComp( this_divE, node_box, 0,
                                      JaE0, 0,
                                      dX[dir], dir,
                                      additive ); 
      this_divE.divide(Jnc[dit].getFab(),0,0,1);
  
   }
   m_field_bc->applyOnAxisDivBC( m_divE, m_electricField );
   
   // now do dir=1:SpaceDim
   for(DataIterator dit(grids); dit.ok(); ++dit) {
         
      FArrayBox& this_divE( m_divE[dit].getFab() );
      Box node_box = grids[dit];
      node_box.surroundingNodes();
         
      int additive = 1;
      for (int dir=1; dir<SpaceDim; dir++) {
         const FArrayBox& this_E( m_electricField[dit][dir] );
         SpaceUtils::simpleStagGradComp( this_divE, node_box, 0,
                                         this_E, 0,
                                         dX[dir], dir,
                                         additive ); 
      }

   }
   
}

void ElectroMagneticFields::setDivB()
{
   CH_TIME("ElectroMagneticFields::setDivB()");
    
   // get some Grid info
   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const LevelData<FluxBox>& Jfc = m_mesh.getJfc();
   const LevelData<FArrayBox>& Jcc = m_mesh.getJcc();
   const RealVect& dX(m_mesh.getdX());
   
   for(DataIterator dit(grids); dit.ok(); ++dit) {
         
      FArrayBox& this_divB( m_divB[dit] );
      const Box& cell_box = grids[dit];
         
      this_divB.setVal(0.0, cell_box, 0, this_divB.nComp());
      int additive = 0;
      for (int dir=0; dir<SpaceDim; dir++) {
         const FArrayBox& this_B( m_magneticField[dit][dir] );
         if(dir==0) {
            FArrayBox JaB0;
            JaB0.define(this_B.box(),1); 
            JaB0.copy(this_B,0,0,1);           
            JaB0.mult(Jfc[dit][dir],0,0,1);           
            SpaceUtils::simpleStagGradComp( this_divB, cell_box, 0,
                                            JaB0, 0,
                                            dX[dir], dir,
                                            additive ); 
            this_divB.divide(Jcc[dit],0,0,1);
         }
         else {
            additive = 1;
            SpaceUtils::simpleStagGradComp( this_divB, cell_box, 0,
                                            this_B, 0,
                                            dX[dir], dir,
                                            additive );
         }
      }
   }
   
}

void ElectroMagneticFields::enforceGaussLaw( const Real  a_time )
{
   CH_TIME("ElectroMagneticFields::enforceGaussLaw()");

   solvePoisson( a_time );
   correctElectricField();

}

void ElectroMagneticFields::solvePoisson( const Real  a_time )
{
   CH_TIME("ElectroMagneticFields::solvePoisson()");

   setDivE();

   // 1) solve for correction potential: nabla^2(phi) = div(E) - rhoC/ep0
   // 2) compute the correction field:   E_corr = -nabla(phi)

   const DisjointBoxLayout& grids(m_mesh.getDBL());
   const int ghosts(m_mesh.ghosts());
   const IntVect ghostVect = ghosts*IntVect::Unit; 
  
   /* 
   GridFunctionFactory  gridFactory;
   LevelData<NodeFArrayBox> temporaryNodeProfile;
   temporaryNodeProfile.define(grids,1,m_electricField_virtual.ghostVect());
   
   string prhs("poisson.rhs"); 
   ParmParse pp( prhs );
   RefCountedPtr<GridFunction> gridFunction = gridFactory.create(pp,m_verbosity);
   gridFunction->assign( temporaryNodeProfile, m_mesh, a_time );
   */

   // set the rhs for container with default dbl
   int numlevels = m_poisson_params.numLevels;
   for(DataIterator dit(grids); dit.ok(); ++dit) {
      Box dst_box = grids[dit];
      dst_box.surroundingNodes();
      FArrayBox& this_rhs = m_rhs0[dit].getFab();
      const FArrayBox& this_divE = m_divE[dit].getFab();
      const FArrayBox& this_rhoC = m_chargeDensity[dit].getFab();
      this_rhs.copy(this_rhoC,dst_box,0,dst_box,0,1);
      this_rhs.mult(-m_rhoCnorm_factor);
      this_rhs.plus(this_divE,dst_box,0,0,1);
      //this_rhs.copy(temporaryNodeProfile[dit].getFab(),dst_box,0,dst_box,0,1);
   }
   SpaceUtils::exchangeNodeFArrayBox(m_rhs0);

   // use copyTo to put rhs on container with dbl used for poisson solver
   LevelData<NodeFArrayBox>& rhs_vect0= *m_rhs_vector[0];
   m_rhs0.copyTo(rhs_vect0);

   // solve for phi
   int lbase = 0; 
   m_poisson_solver.solve(m_phi_vector, m_rhs_vector, numlevels-1, lbase);
   
   // use copyTo to fill phi container with default dbl
   LevelData<NodeFArrayBox>& phi_vect0= *m_phi_vector[0];
   phi_vect0.copyTo(m_phi0);
      
   // compute E_corr = -nabla(phi)
   const RealVect& dX(m_mesh.getdX());
   for(DataIterator dit(grids); dit.ok(); ++dit) {
      FArrayBox& phi = m_phi0[dit].getFab();
      for (int dir=0; dir<SpaceDim; dir++) {
         FArrayBox& deltaE = m_electricField_correction[dit][dir];
         const Box& edge_box = deltaE.box();
            
         int additive = 0;
         SpaceUtils::simpleStagGradComp( deltaE, edge_box, 0,
                                         phi, 0,
                                         -dX[dir], dir,
                                         additive ); 
      }
   }
   SpaceUtils::exchangeEdgeDataBox(m_electricField_correction);

}

void ElectroMagneticFields::correctElectricField()
{
   CH_TIME("ElectroMagneticFields::correctElectricField()");
   const DisjointBoxLayout& grids(m_mesh.getDBL());
  
   // add E_corr = -nabla(phi) to E
   for(DataIterator dit(grids); dit.ok(); ++dit) {
      for (int dir=0; dir<SpaceDim; dir++) {
         FArrayBox& Edir = m_electricField[dit][dir];
         const FArrayBox& deltaE = m_electricField_correction[dit][dir];
         const Box& edge_box = deltaE.box();
         Edir.plus(deltaE,edge_box,0,0,1);
      }
   }

}   
   
void ElectroMagneticFields::assemblePrecondMatrix( BandedMatrix&              a_P,
                                             const bool                       a_use_mass_matrices,
                                             const Real                       a_cnormDt )
{
  CH_TIME("ElectroMagneticFields::assemblePrecondMatrix()");

  auto grids(m_mesh.getDBL());

  // copy over the mass matrix elements used in the PC and do addOp exchange() 
  const Real sig_normC = a_cnormDt*m_Jnorm_factor;
  int diag_comp_inPlane=0, diag_comp_virtual=0; 
  if(a_use_mass_matrices) {
    diag_comp_inPlane = (m_sigma_xx_pc.nComp()-1)/2;
#if CH_SPACEDIM<3
    diag_comp_virtual = (m_sigma_zz_pc.nComp()-1)/2;
#endif
  }

  const LevelData<FluxBox>& pmap_B_lhs = m_gdofs.dofDataLHSB();
  const LevelData<FArrayBox>& pmap_Bv_lhs = m_gdofs.dofDataLHSBv();
  const LevelData<EdgeDataBox>& pmap_E_lhs = m_gdofs.dofDataLHSE();
  const LevelData<NodeFArrayBox>& pmap_Ev_lhs = m_gdofs.dofDataLHSEv();
  
#if CH_SPACEDIM>1
  const LevelData<FluxBox>& pmap_B_rhs = m_gdofs.dofDataRHSB();
#endif
  const LevelData<FArrayBox>& pmap_Bv_rhs = m_gdofs.dofDataRHSBv();
  const LevelData<EdgeDataBox>& pmap_E_rhs = m_gdofs.dofDataRHSE();
  const LevelData<NodeFArrayBox>& pmap_Ev_rhs = m_gdofs.dofDataRHSEv();
  
  const LevelData<FArrayBox>& Xcc(m_mesh.getXcc());
#if CH_SPACEDIM==1
  const LevelData<NodeFArrayBox>& Xnc(m_mesh.getXnc());
#elif CH_SPACEDIM==2
  const LevelData<EdgeDataBox>& Xec(m_mesh.getXec());
#endif

#if CH_SPACEDIM<3
  const LevelData<NodeFArrayBox>& Jnc = m_mesh.getCorrectedJnc();  
#endif
  const LevelData<EdgeDataBox>& Jec = m_mesh.getCorrectedJec();  

  auto phys_domain( grids.physDomain() );
  const int ghosts(m_mesh.ghosts());
  const RealVect& dX(m_mesh.getdX());

#define FILTER_PC_TEST

  for (auto dit( grids.dataIterator() ); dit.ok(); ++dit) {

    /* magnetic field */
    for (int dir = 0; dir < SpaceDim; dir++) {

      const FArrayBox& pmap( pmap_B_lhs[dit][dir] );

      auto box( grow(pmap.box(), -ghosts) );
      int idir_bdry = phys_domain.domainBox().bigEnd(dir);
      if (box.bigEnd(dir) < idir_bdry) box.growHi(dir, -1);

      for (BoxIterator bit(box); bit.ok(); ++bit) {

        auto ic = bit();
        CH_assert(pmap.nComp() == 1);
        int pc = (int) pmap(ic, 0);
        
        int ncols = m_pcmat_nbands;
        std::vector<int> icols(ncols, -1);
        std::vector<Real> vals(ncols, 0.0);
        int ix = 0;

        icols[ix] = pc; 
        vals[ix] = 1.0;
        ix++;

#if CH_SPACEDIM==2
        int tdir = (dir + 1) % SpaceDim;
        int sign = 1 - 2*dir;
        if(m_mesh.anticyclic()) sign *= -1;

        IntVect iL(ic);
        Real aL = (-1) * sign * (-a_cnormDt/dX[tdir]) * m_advanceB_comp[dir];
        int pL = (int) pmap_Ev_rhs[dit](iL, 0);

        if (pL >= 0) {
          icols[ix] = pL;
          vals[ix] = -aL;
          ix++;
        }

        IntVect iR(ic); iR[tdir]++;
        Real aR = sign * (-a_cnormDt/dX[tdir]) * m_advanceB_comp[dir];
        int pR = (int) pmap_Ev_rhs[dit](iR, 0);

        if (pR >= 0) {
          icols[ix] = pR;
          vals[ix] = -aR;
          ix++;
        }
#endif

        CH_assert(ix <= m_pcmat_nbands);
        CH_assert(ix <= a_P.getNBands());

        a_P.setRowValues( pc, ix, icols.data(), vals.data() );
      }
    }

    /* electric field */
    for (int dir = 0; dir < SpaceDim; dir++) {

      const FArrayBox& pmap( pmap_E_lhs[dit][dir] );

      auto box( grow(pmap.box(), -ghosts) );
      for (int adir=0; adir<SpaceDim; ++adir) {
         if (adir != dir) {
            int idir_bdry = phys_domain.domainBox().bigEnd(adir);
            if (box.bigEnd(adir) < idir_bdry) box.growHi(adir, -1);
         }
      }

      for (BoxIterator bit(box); bit.ok(); ++bit) {

        auto ic = bit();
        CH_assert( pmap.nComp() == 1);
        int pc = (int) pmap(ic, 0);
              
	const Real sig_norm_ec = sig_normC/Jec[dit][dir](ic,0);
        
        int ncols = m_pcmat_nbands;
        std::vector<int> icols(ncols, -1);
        std::vector<Real> vals(ncols, 0.0);

        int ix = 0;
        icols[ix] = pc; 
        vals[ix] = 1.0;
        if(a_use_mass_matrices) { // dJ_{ic}/dE_{ic}
#ifdef FILTER_PC_TEST  
           if(m_use_filtering) {
              vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-1);
              vals[ix] += sig_norm_ec*4.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane);
              vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+1);
              IntVect iL(ic); iL[0] -= 1;
	      int pL = (int) pmap_E_rhs[dit][dir](iL,0);
	      if(pL>=0) {
		 vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iL,diag_comp_inPlane);
		 vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](iL,diag_comp_inPlane+1);
		 vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iL,diag_comp_inPlane+2);
	      }
              IntVect iR(ic); iR[0] += 1;
	      int pR = (int) pmap_E_rhs[dit][dir](iR,0);
	      if(pR>=0) {
		 vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iR,diag_comp_inPlane-2);
		 vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](iR,diag_comp_inPlane-1);
		 vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iR,diag_comp_inPlane);
	      }
	   }
	   else {
              vals[ix] += sig_norm_ec*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane);
	   }
#else

           vals[ix] += sig_norm_ec*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane);
#endif
        }
        ix++;

#if CH_SPACEDIM==1
        if (a_use_mass_matrices && m_advanceE_comp[0]) {

          /* sigma_xx contributions */
          for (int n = 1; n < m_pc_mass_matrix_width+1; n++) {

            if (n > (m_sigma_xx_pc.nComp()-1)/2) break;

            IntVect iL(ic); iL[0] -= n;
            int pL = (int) pmap_E_rhs[dit][dir](iL, 0);
            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = 0.0;
#ifdef FILTER_PC_TEST   
              if(m_use_filtering && n==1) { // dJ_{ic}/dE_{ic-1}
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iL,diag_comp_inPlane-1);
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](iL,diag_comp_inPlane);
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iL,diag_comp_inPlane+1);
		//  
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-2);
                vals[ix] += sig_norm_ec*4.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-1);
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane);
	        //
                IntVect ip(ic); ip[0] += 1;
                int pR = (int) pmap_E_rhs[dit][dir](ip, 0);
	        if(pR >= 0) {
		  vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane-2);
		  vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane-1);
	        }
	      }
	      else if(m_use_filtering && n==2) { // dJ_{ic}/dE_{ic-2}
                IntVect im(ic); im[0] -= 1;
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane-2);
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane-1);
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane);
		//
                vals[ix] += sig_norm_ec*4.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-2);
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-1);
                //
		IntVect ip(ic); ip[0] += 1;
                int pR = (int) pmap_E_rhs[dit][dir](ip, 0);
	        if(pR >= 0) {
                  vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane-2);
		}
	      }
	      else if(m_use_filtering && n==3) { // dJ_{ic}/dE_{ic-3}
                IntVect im(ic); im[0] -= 1;
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane-2);
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane-1);
		//
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-2);
	      }
	      else if(m_use_filtering && n==4) { // dJ_{ic}/dE_{ic-4}
                IntVect im(ic); im[0] -= 1;
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane-2);
	      }
	      else {
                vals[ix] = sig_norm_ec*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-n);
	      }
#else
              vals[ix] = sig_norm_ec*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane-n);
#endif
              ix++;
            }
            
            IntVect iR(ic); iR[0] += n;
            int pR = (int) pmap_E_rhs[dit][dir](iR, 0);
            if (pR >= 0) { 
              icols[ix] = pR;
              vals[ix] = 0.0;
#ifdef FILTER_PC_TEST   
              if(m_use_filtering && n==1) { // dJ_{ic}/dE_{ic+1}
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane);
                vals[ix] += sig_norm_ec*4.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+1);
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+2);
		//
                IntVect im(ic); im[0] -= 1;
                int pL = (int) pmap_E_rhs[dit][dir](im, 0);
	        if(pL >= 0) {
		  vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane+1);
		  vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane+2);
	        }
		//
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iR,diag_comp_inPlane-1);
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](iR,diag_comp_inPlane);
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](iR,diag_comp_inPlane+1);
	      }
	      else if(m_use_filtering && n==2) { // dJ_{ic}/dE_{ic+2}
                IntVect im(ic); im[0] -= 1;
                int pL = (int) pmap_E_rhs[dit][dir](im, 0);
	        if(pL >= 0) {
                  vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](im,diag_comp_inPlane+2);
		}
		//
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+1);
                vals[ix] += sig_norm_ec*4.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+2);
		//  
                IntVect ip(ic); ip[0] += 1;
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane);
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane+1);
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane+2);
	      }
	      else if(m_use_filtering && n==3) { // dJ_{ic}/dE_{ic+3}
                vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+2);
		//  
                IntVect ip(ic); ip[0] += 1;
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane+1);
		vals[ix] += sig_norm_ec*2.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane+2);
	      }
	      else if(m_use_filtering && n==4) { // dJ_{ic}/dE_{ic+4}
                IntVect ip(ic); ip[0] += 1;
		vals[ix] += sig_norm_ec*1.0/16.0*m_sigma_xx_pc[dit][dir](ip,diag_comp_inPlane+2);
	      }
	      else {
                vals[ix] = sig_norm_ec*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+n);
	      }
#else
              vals[ix] = sig_norm_ec*m_sigma_xx_pc[dit][dir](ic,diag_comp_inPlane+n);
#endif
              ix++;
            }
          }

          if (m_pc_mass_matrix_include_ij) {

            /* sigma_xy contributions */
            for (int n = 0; n < m_pc_mass_matrix_width; n++) {
  
              if (n > m_sigma_xy_pc.nComp()/2-1) break;
  
              IntVect iL(ic); iL[0] -= n;
              int pL = (int) pmap_Ev_rhs[dit](iL,0);
              if (pL >= 0) {
                icols[ix] = pL;
                vals[ix] = sig_norm_ec*m_sigma_xy_pc[dit][dir](ic,m_sigma_xy_pc.nComp()/2-1-n);
                ix++;
              }
  
              IntVect iR(ic); iR[0] += (n+1);
              int pR = (int) pmap_Ev_rhs[dit](iR,0);
              if (pR >= 0) {
                icols[ix] = pR;
                vals[ix] = sig_norm_ec*m_sigma_xy_pc[dit][dir](ic,m_sigma_xy_pc.nComp()/2+n);
                ix++;
              }
  
            }
  
            /* sigma_xz contributions */
            for (int n = 0; n < m_pc_mass_matrix_width; n++) {
  
              if (n > m_sigma_xz_pc.nComp()/2-1) break;
  
              IntVect iL(ic); iL[0] -= n;
              int pL = (int) pmap_Ev_rhs[dit](iL,1);
              if (pL >= 0) {
                icols[ix] = pL;
                vals[ix] = sig_norm_ec*m_sigma_xz_pc[dit][dir](ic,m_sigma_xz_pc.nComp()/2-1-n);
                ix++;
              }
  
              IntVect iR(ic); iR[0] += (n+1);
              int pR = (int) pmap_Ev_rhs[dit](iR,1);
              if (pR >= 0) {
                icols[ix] = pR;
                vals[ix] = sig_norm_ec*m_sigma_xz_pc[dit][dir](ic,m_sigma_xz_pc.nComp()/2+n);
                ix++;
              }
  
            }
          }
        }
#endif
        
#if CH_SPACEDIM==2 
        int tdir = (dir + 1) % SpaceDim;
        int sign = 1 - 2*dir;
        if(m_mesh.anticyclic()) sign *= -1;
            
	// JRA, here is where axisymmetric modifications are needed in 2D RZ
        // dEz_{i,j+1/2}/dt + Jz_{i,j+1/2} = (r_{i+1/2}*Bth_{i+1/2,j+1/2} - r_{i-1/2}*Bth_{i-1/2,j+1/2})/dr/r_{i}
        // for r_{i=0}=0, d(rBth)/dr/r = 4*Bth_{i=1/2,j+1/2}/dr

        IntVect iL(ic); iL[tdir]--;
        Real aL = (-1) * sign * (a_cnormDt/dX[tdir]) * m_advanceE_comp[dir];
        int pL = (int) pmap_Bv_rhs[dit](iL, 0);

        if (pL >= 0) {
          icols[ix] = pL;
          if(m_mesh.axisymmetric() && dir==1) {
            Real local_Xec = Xec[dit][dir](ic,0);
            if(local_Xec==0.0) vals[ix] = 0.0;
            else vals[ix] = -aL*Xcc[dit](iL,0)/local_Xec;
          }
          else vals[ix] = -aL;
          ix++;
        }

        IntVect iR(ic);
        Real aR = sign * (a_cnormDt/dX[tdir]) * m_advanceE_comp[dir];
        int pR = (int) pmap_Bv_rhs[dit](iR, 0);

        if (pR >= 0) {
          icols[ix] = pR;
          if(m_mesh.axisymmetric() && dir==1) {
            Real local_Xec = Xec[dit][dir](ic,0);
            if(local_Xec==0.0) vals[ix] = -aR*4.0;
            else vals[ix] = -aR*Xcc[dit](iR,0)/local_Xec;
          }
          else vals[ix] = -aR;
          ix++;
        }
#endif

        CH_assert(ix <= m_pcmat_nbands);
        CH_assert(ix <= a_P.getNBands());

        a_P.setRowValues( pc, ix, icols.data(), vals.data() );
      }
    }

#if CH_SPACEDIM<3 

    /* virtual magnetic field */
    {
      const FArrayBox& pmap( pmap_Bv_lhs[dit] );
      auto box( grow(pmap.box(), -ghosts) );
  
      for (BoxIterator bit(box); bit.ok(); ++bit) {
  
        auto ic = bit();
  
        for (int n(0); n < pmap.nComp(); n++) {
          int pc = (int) pmap(ic, n);
            
          int ncols = m_pcmat_nbands;
          std::vector<int> icols(ncols, -1);
          std::vector<Real> vals(ncols, 0.0);
          int ix = 0;

          icols[ix] = pc; 
          vals[ix] = 1.0;
          ix++;

#if CH_SPACEDIM==1 

          int dirj = 0, dirk = 1;

          if (n == 0) {

            int sign = -1;

            Real aL = (-1) * sign * (-a_cnormDt/dX[0]) * m_advanceB_comp[n+1];
            IntVect iL(ic);
            int pL = (int) pmap_Ev_rhs[dit](iL, dirk);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR =  sign * (-a_cnormDt/dX[0]) * m_advanceB_comp[n+1];
            IntVect iR(ic); iR[0]++;
            int pR = (int) pmap_Ev_rhs[dit](iR, dirk);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }

          } else if (n == 1) {

            int sign = 1;

            Real aL = (-1) * sign * (-a_cnormDt/dX[0]) * m_advanceB_comp[n+1];
            IntVect iL(ic);
            int pL = (int) pmap_Ev_rhs[dit](iL, dirj);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR =  sign * (-a_cnormDt/dX[0]) * m_advanceB_comp[n+1];
            IntVect iR(ic); iR[0]++;
            int pR = (int) pmap_Ev_rhs[dit](iR, dirj);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }

          }

#elif CH_SPACEDIM==2

          int dirj = 0, dirk = 1;

          {
            int sign = 1;
            if(m_mesh.anticyclic()) sign *= -1;

            Real aL = (-1) * sign * (-a_cnormDt/dX[dirj]) * m_advanceB_comp[2];
            IntVect iL(ic);
            int pL = (int) pmap_E_rhs[dit][dirk](iL, 0);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR = sign * (-a_cnormDt/dX[dirj]) * m_advanceB_comp[2];
            IntVect iR(ic); iR[dirj]++;
            int pR = (int) pmap_E_rhs[dit][dirk](iR, 0);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }
          }
          {
            int sign = -1;
            if(m_mesh.anticyclic()) sign *= -1;

            Real aL = (-1) * sign * (-a_cnormDt/dX[dirk]) * m_advanceB_comp[2];
            IntVect iL(ic);
            int pL = (int) pmap_E_rhs[dit][dirj](iL, 0);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR = sign * (-a_cnormDt/dX[dirk]) * m_advanceB_comp[2];
            IntVect iR(ic); iR[dirk]++;
            int pR = (int) pmap_E_rhs[dit][dirj](iR, 0);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }
          }
#endif
          CH_assert(ix <= m_pcmat_nbands);
          CH_assert(ix <= a_P.getNBands());
  
          a_P.setRowValues( pc, ix, icols.data(), vals.data() );
        }
      }
    }
  
    /* virtual electric field */
    {
      const NodeFArrayBox& pmap( pmap_Ev_lhs[dit] );

      auto box( surroundingNodes( grow(pmap.box(), -ghosts) ) );
      for (int dir=0; dir<SpaceDim; ++dir) {
         int idir_bdry = phys_domain.domainBox().bigEnd(dir);
         if (box.bigEnd(dir) < idir_bdry) box.growHi(dir, -1);
      }
  
      for (BoxIterator bit(box); bit.ok(); ++bit) {
  
        auto ic = bit();
        const Real sig_norm_nc = sig_normC/Jnc[dit](ic,0);
  
        for (int n(0); n < pmap.nComp(); n++) {

          int pc = (int) pmap(ic, n);
            
          int ncols = m_pcmat_nbands;
          std::vector<int> icols(ncols, -1);
          std::vector<Real> vals(ncols, 0.0);
          int ix = 0;

          icols[ix] = pc; 
          vals[ix] = 1.0;
#if CH_SPACEDIM==1
          if(a_use_mass_matrices && m_advanceE_comp[1+n]) {
              vals[ix] += sig_norm_nc * ( n==0 ? m_sigma_yy_pc[dit](ic,diag_comp_virtual)
                                               : m_sigma_zz_pc[dit](ic,diag_comp_virtual) );
          }
#elif CH_SPACEDIM==2
          if(a_use_mass_matrices && m_advanceE_comp[2]) {
            vals[ix] += sig_norm_nc*m_sigma_zz_pc[dit](ic,diag_comp_virtual);
          }
#endif
          ix++;

          if(a_use_mass_matrices && m_advanceE_comp[SpaceDim+n]) {
#if CH_SPACEDIM==1
            /* sigma_yy/zz contributions */
            for (int s = 1; s < m_pc_mass_matrix_width+1; s++) {

              if (s > (m_sigma_yy_pc.nComp()-1)/2) break;
              if (s > (m_sigma_zz_pc.nComp()-1)/2) break;

              IntVect iL(ic); iL[0] -= s;
              int pL = (int) pmap_Ev_rhs[dit](iL, n);
              if (pL >= 0) {
                icols[ix] = pL;
                vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yy_pc[dit](ic,diag_comp_virtual-s)
                                                : m_sigma_zz_pc[dit](ic,diag_comp_virtual-s) );
                ix++;
              }
                
              IntVect iR(ic); iR[0] += s;
              int pR = (int) pmap_Ev_rhs[dit](iR, n);
              if (pR >= 0) { 
                icols[ix] = pR;
                vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yy_pc[dit](ic,diag_comp_virtual+s)
                                                : m_sigma_zz_pc[dit](ic,diag_comp_virtual+s) );
                ix++;
              }
            }
             
            if (m_pc_mass_matrix_include_ij) {

              // sigma_yx/zx contributions
              for (int s = 0; s < m_pc_mass_matrix_width; s++) {
  
                if (s > m_sigma_yx_pc.nComp()/2-1) break;
  
                IntVect iL(ic); iL[0] -= (s+1);
                int pL = (int) pmap_E_rhs[dit][0](iL,0);
                if (pL >= 0) {
                  icols[ix] = pL;
                  vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yx_pc[dit](ic,m_sigma_yx_pc.nComp()/2-1-s)
                                                  : m_sigma_zx_pc[dit](ic,m_sigma_zx_pc.nComp()/2-1-s) );
                  ix++;
                }
  
                IntVect iR(ic); iR[0] += s;
                int pR = (int) pmap_E_rhs[dit][0](iR,0);
                if (pR >= 0) {
                  icols[ix] = pR;
                  vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yx_pc[dit](ic,m_sigma_yx_pc.nComp()/2+s)
                                                  : m_sigma_zx_pc[dit](ic,m_sigma_zx_pc.nComp()/2+s) );
                  ix++;
                }
  
              }
  
              // sigma_yz/zy contributions
              {
                IntVect ii(ic);
                int pI = (int) pmap_Ev_rhs[dit](ii, !n);
                if (pI >= 0) {
                  icols[ix] = pI;
                  vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yz_pc[dit](ic,(m_sigma_yz_pc.nComp()-1)/2)
                                                  : m_sigma_zy_pc[dit](ic,(m_sigma_zy_pc.nComp()-1)/2) );
                  ix++;
                }
              }
              for (int s = 1; s < m_pc_mass_matrix_width+1; s++) {
  
                if (s > (m_sigma_yz_pc.nComp()-1)/2) break;
  
                IntVect iL(ic); iL[0] -= s;
                int pL = (int) pmap_Ev_rhs[dit](iL, !n);
                if (pL >= 0) {
                  icols[ix] = pL;
                  vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yz_pc[dit](ic,(m_sigma_yz_pc.nComp()-1)/2-s)
                                                  : m_sigma_zy_pc[dit](ic,(m_sigma_zy_pc.nComp()-1)/2-s) );
                  ix++;
                }
  
                IntVect iR(ic); iR[0] += s;
                int pR = (int) pmap_Ev_rhs[dit](iR, !n);
                if (pR >= 0) {
                  icols[ix] = pR;
                  vals[ix] = sig_norm_nc * ( n==0 ? m_sigma_yz_pc[dit](ic,(m_sigma_yz_pc.nComp()-1)/2+s)
                                                  : m_sigma_zy_pc[dit](ic,(m_sigma_zy_pc.nComp()-1)/2+s) );
                  ix++;
                }
              }
  
            }
#endif
          }
    
#if CH_SPACEDIM==1 

          int dirj = 0, dirk = 1;

          if (n == 0) {

            int sign = -1;

            Real aL = (-1) * sign * (a_cnormDt/dX[0]) * m_advanceE_comp[n+1];
            IntVect iL(ic); iL[0]--;
            int pL = (int) pmap_Bv_rhs[dit](iL, dirk);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR =  sign * (a_cnormDt/dX[0]) * m_advanceE_comp[n+1];
            IntVect iR(ic);
            int pR = (int) pmap_Bv_rhs[dit](iR, dirk);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }

          } else if (n == 1) {

            int sign = 1;

            // JRA, here is where axisymmetric modifications are needed in 1D
            // dEz_{i}/dt + Jz_{i} = (r_{i+1/2}*Bth_{i+1/2} - r_{i-1/2}*Bth_{i-1/2})/dr/r_{i}
            // for r_{i=0}=0, d(rBth)/dr/r = 4*Bth_{i=1/2}/dr

            Real aL = (-1) * sign * (a_cnormDt/dX[0]) * m_advanceE_comp[n+1];
            IntVect iL(ic); iL[0]--;
            int pL = (int) pmap_Bv_rhs[dit](iL, dirj);

            if (pL >= 0) {
              icols[ix] = pL;
              if(m_mesh.axisymmetric()) {
                Real local_Xnc = Xnc[dit](ic,0);
                if(local_Xnc==0.0) vals[ix] = 0.0;
                else vals[ix] = -aL*Xcc[dit](iL,0)/local_Xnc;
              }
              else vals[ix] = -aL;
              ix++;
            }

            Real aR =  sign * (a_cnormDt/dX[0]) * m_advanceE_comp[n+1];
            IntVect iR(ic);
            int pR = (int) pmap_Bv_rhs[dit](iR, dirj);

            if (pR >= 0) {
              icols[ix] = pR;
              if(m_mesh.axisymmetric()) {
                Real local_Xnc = Xnc[dit](ic,0);
                if(local_Xnc==0.0) vals[ix] = -aR*4.0;
                else vals[ix] = -aR*Xcc[dit](iR,0)/local_Xnc;
              }
              else vals[ix] = -aR;
              ix++;
            }

          }

#elif CH_SPACEDIM==2 

          int dirj = 0, dirk = 1;

          {
            int sign = 1;
            if(m_mesh.anticyclic()) sign *= -1;

            Real aL = (-1) * sign * (a_cnormDt/dX[dirj]) * m_advanceE_comp[2];
            IntVect iL(ic); iL[dirj]--;
            int pL = (int) pmap_B_rhs[dit][dirk](iL, 0);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR = sign * (a_cnormDt/dX[dirj]) * m_advanceE_comp[2];
            IntVect iR(ic);
            int pR = (int) pmap_B_rhs[dit][dirk](iR, 0);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }
          }
          {
            int sign = -1;
            if(m_mesh.anticyclic()) sign *= -1;

            Real aL = (-1) * sign * (a_cnormDt/dX[dirk]) * m_advanceE_comp[2];
            IntVect iL(ic); iL[dirk]--;
            int pL = (int) pmap_B_rhs[dit][dirj](iL, 0);

            if (pL >= 0) {
              icols[ix] = pL;
              vals[ix] = -aL;
              ix++;
            }

            Real aR = sign * (a_cnormDt/dX[dirk]) * m_advanceE_comp[2];
            IntVect iR(ic);
            int pR = (int) pmap_B_rhs[dit][dirj](iR, 0);

            if (pR >= 0) {
              icols[ix] = pR;
              vals[ix] = -aR;
              ix++;
            }
          }
#endif
    
          CH_assert(ix <= m_pcmat_nbands);
          CH_assert(ix <= a_P.getNBands());
  
          a_P.setRowValues( pc, ix, icols.data(), vals.data() );
        }
      }
    }
#endif

  }

  return;
}

#include "NamespaceFooter.H"

