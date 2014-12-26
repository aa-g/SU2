/*!
 * \file output_tecplot.cpp
 * \brief Main subroutines for output solver information.
 * \author F. Palacios, T. Economon, M. Colonno
 * \version 3.2.6 "eagle"
 *
 * Copyright (C) 2012-2014 SU2 <https://github.com/su2code>.
 *
 * SU2 is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SU2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with SU2. If not, see <http://www.gnu.org/licenses/>.
 */

#include "../include/output_structure.hpp"

string AssembleVariableNames(CGeometry *geometry, CConfig *config, unsigned short nVar_Consv, unsigned short *NVar);

void COutput::SetTecplotNode_ASCII(CConfig *config, CGeometry *geometry, CSolver **solver, char mesh_filename[MAX_STRING_SIZE], bool surf_sol) {
  
  int rank = MASTER_NODE;
#ifdef HAVE_MPI
  int size = SINGLE_NODE;
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif
  
  unsigned long iElem, iPoint;
  unsigned short iVar;
  bool grid_movement  = config->GetGrid_Movement();
  unsigned short Kind_Solver = config->GetKind_Solver();
  ofstream Tecplot_File;
  unsigned long Total_nElem_Bound, *PointSurface = NULL, nPointSurface = 0;
  unsigned short iMarker;
  
  /*--- Open Tecplot ASCII file and write the header. ---*/

  Tecplot_File.open(mesh_filename, ios::out);
  Tecplot_File.precision(6);
  if (surf_sol) Tecplot_File << "TITLE = \"Visualization of the surface solution\"" << endl;
  else Tecplot_File << "TITLE = \"Visualization of the volumetric solution\"" << endl;
  
  /*--- Write the list of the fields in the restart file.
   Without including the PointID---*/
  if (config->GetKind_SU2() == SU2_SOL) {
    
    /*--- If SU2_SOL called this routine, we already have a set of output
     variables with the appropriate string tags stored in the config class. ---*/
    Tecplot_File << "VARIABLES = ";
    nVar_Total = config->fields.size() - 1;
    for (unsigned short iField = 1; iField < config->fields.size(); iField++) {
      Tecplot_File << config->fields[iField];
    }
    
    Tecplot_File << endl;
    
  } else {
    
    if (geometry->GetnDim() == 2) { Tecplot_File << "VARIABLES = \"x\",\"y\""; }
    else { Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\""; }
    
    /*--- Add names for conservative, limiters, and residual variables ---*/
    
    for (iVar = 0; iVar < nVar_Consv; iVar++) {
      Tecplot_File << ",\"Conservative_" << iVar+1 << "\"";
    }
    
    if (!config->GetLow_MemoryOutput()) {
      
      if (config->GetWrt_Limiters()) {
        for (iVar = 0; iVar < nVar_Consv; iVar++) {
          Tecplot_File << ",\"Limiter_" << iVar+1 << "\"";
        }
      }
      if (config->GetWrt_Residuals()) {
        for (iVar = 0; iVar < nVar_Consv; iVar++) {
          Tecplot_File << ",\"Residual_" << iVar+1 << "\"";
        }
      }
      
      /*--- Add names for any extra variables (this will need to be adjusted). ---*/
      
      if (grid_movement) {
        if (geometry->GetnDim() == 2) {
          Tecplot_File << ",\"Grid_Velx\",\"Grid_Vely\"";
        } else {
          Tecplot_File << ",\"Grid_Velx\",\"Grid_Vely\",\"Grid_Velz\"";
        }
      }
      
      if ((Kind_Solver == EULER) || (Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
        Tecplot_File << ",\"Pressure\",\"Temperature\",\"Pressure_Coefficient\",\"Mach\"";
        if ((Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
          Tecplot_File << ",\"Laminar_Viscosity\", \"Skin_Friction_Coefficient\", \"Heat_Flux\", \"Y_Plus\"";
          if (Kind_Solver == RANS) { Tecplot_File << ", \"Eddy_Viscosity\""; }
        }
        if (config->GetWrt_SharpEdges()) { Tecplot_File << ", \"Sharp_Edge_Dist\""; }
      }
      
      if ((Kind_Solver == ADJ_EULER) || (Kind_Solver == ADJ_NAVIER_STOKES) || (Kind_Solver == ADJ_RANS) ) {
        Tecplot_File << ", \"Surface_Sensitivity\", \"Solution_Sensor\"";
      }
      
    }

    Tecplot_File << endl;

  }
  
  
  if (surf_sol) {
    
    /*--- It is important to do a renumbering to don't add points
     that do not belong to the surfaces ---*/
    
    PointSurface = new unsigned long[geometry->GetnPoint()];
    for (iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++)
      if (geometry->node[iPoint]->GetBoundary()) {
        PointSurface[iPoint] = nPointSurface;
        nPointSurface++;
      }
    
    /*--- Compute the total number of elements ---*/
    
    Total_nElem_Bound = 0;
    for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++) {
      if (config->GetMarker_All_Plotting(iMarker) == YES) {
        Total_nElem_Bound += geometry->GetnElem_Bound(iMarker);
      }
    }
    
    if (Total_nElem_Bound != 0) {
      
      /*--- Write the header of the file ---*/

      Tecplot_File << "ZONE T= \"MPI rank: " << rank << "\", ";
      Tecplot_File << "NODES= "<< nPointSurface <<", ELEMENTS= "<< Total_nElem_Bound <<", DATAPACKING= POINT";
      if (geometry->GetnDim() == 2) Tecplot_File << ", ZONETYPE= FELINESEG"<< endl;
      if (geometry->GetnDim() == 3) Tecplot_File << ", ZONETYPE= FEQUADRILATERAL"<< endl;
      
      /*--- Only write the coordiantes of the points that are on the surfaces ---*/
      
      if (geometry->GetnDim() == 3) {
        for(iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++)
          if (geometry->node[iPoint]->GetBoundary()) {
            for(iVar = 0; iVar < solver[FLOW_SOL]->GetnVar(); iVar++)
              Tecplot_File << scientific << solver[FLOW_SOL]->node[iPoint]->GetSolution(iVar) << "\t";
            Tecplot_File << "\n";
          }
      }
      else {
        for(iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++)
          if (geometry->node[iPoint]->GetBoundary()) {
            for(iVar = 0; iVar < solver[FLOW_SOL]->GetnVar(); iVar++)
              Tecplot_File << scientific << solver[FLOW_SOL]->node[iPoint]->GetSolution(iVar) << "\t";
            Tecplot_File << "\n";
          }
      }
      
      /*--- Write the cells using the new numbering ---*/
      
      for (iMarker = 0; iMarker < config->GetnMarker_All(); iMarker++)
        if (config->GetMarker_All_Plotting(iMarker) == YES)
          for(iElem = 0; iElem < geometry->GetnElem_Bound(iMarker); iElem++) {
            if (geometry->GetnDim() == 2) {
              Tecplot_File << PointSurface[geometry->bound[iMarker][iElem]->GetNode(0)]+1 << " "
              << PointSurface[geometry->bound[iMarker][iElem]->GetNode(1)]+1 << endl;
            }
            if (geometry->GetnDim() == 3) {
              if (geometry->bound[iMarker][iElem]->GetnNodes() == 3) {
                Tecplot_File << PointSurface[geometry->bound[iMarker][iElem]->GetNode(0)]+1 << " "
                << PointSurface[geometry->bound[iMarker][iElem]->GetNode(1)]+1 << " "
                << PointSurface[geometry->bound[iMarker][iElem]->GetNode(2)]+1 << " "
                << PointSurface[geometry->bound[iMarker][iElem]->GetNode(2)]+1 << endl;
              }
              if (geometry->bound[iMarker][iElem]->GetnNodes() == 4) {
                Tecplot_File << PointSurface[geometry->bound[iMarker][iElem]->GetNode(0)]+1 << " "
                << PointSurface[geometry->bound[iMarker][iElem]->GetNode(1)]+1 << " "
                << PointSurface[geometry->bound[iMarker][iElem]->GetNode(2)]+1 << " "
                << PointSurface[geometry->bound[iMarker][iElem]->GetNode(3)]+1 << endl;
              }
            }
          }
    }
    else {
      
      /*--- No elements in the surface ---*/
      
      if (geometry->GetnDim() == 2) {
        Tecplot_File << "ZONE ";
        Tecplot_File << "T= \"MPI rank: " << rank << "\", ";
        Tecplot_File << "NODES= 1, ELEMENTS= 1, DATAPACKING=POINT, ZONETYPE=FELINESEG"<< endl;
        for(iVar = 0; iVar < solver[FLOW_SOL]->GetnVar(); iVar++)
          Tecplot_File << scientific << "0.0\t";
        Tecplot_File << "\n";
        Tecplot_File << "1 1"<< endl;
      }
      if (geometry->GetnDim() == 3) {
        Tecplot_File << "ZONE ";
        Tecplot_File << "T= \"MPI rank: " << rank << "\", ";
        Tecplot_File << "NODES= 1, ELEMENTS= 1, DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
        for(iVar = 0; iVar < solver[FLOW_SOL]->GetnVar(); iVar++)
          Tecplot_File << scientific << "0.0\t";
        Tecplot_File << "\n";
        Tecplot_File << "1 1 1 1"<< endl;
      }
    }
    
    /*--- Dealocate memory and close the file ---*/
    
    delete[] PointSurface;
    Tecplot_File.close();
    
  }
  
  else {
    
    Tecplot_File << "ZONE ";
    Tecplot_File << "T= \"MPI rank: " << rank << "\", ";
    Tecplot_File << "NODES= "<< geometry->GetnPoint() <<", ELEMENTS= "<< geometry->GetnElem() <<", DATAPACKING= POINT";
    if (geometry->GetnDim() == 2) Tecplot_File << ", ZONETYPE= FEQUADRILATERAL"<< endl;
    if (geometry->GetnDim() == 3) Tecplot_File << ", ZONETYPE= FEBRICK"<< endl;
    
    /*--- Adding coordinates ---*/
    
    for(iPoint = 0; iPoint < geometry->GetnPoint(); iPoint++) {
      for(iVar = 0; iVar < solver[FLOW_SOL]->GetnVar(); iVar++)
        Tecplot_File << scientific << solver[FLOW_SOL]->node[iPoint]->GetSolution(iVar) << "\t";
      Tecplot_File << "\n";
    }
    
    /*--- Adding conectivity ---*/
    
    for(iElem = 0; iElem < geometry->GetnElem(); iElem++) {
      if (geometry->elem[iElem]->GetVTK_Type() == TRIANGLE) {
        Tecplot_File <<
        geometry->elem[iElem]->GetNode(0)+1 <<" "<< geometry->elem[iElem]->GetNode(1)+1 <<" "<<
        geometry->elem[iElem]->GetNode(2)+1 <<" "<< geometry->elem[iElem]->GetNode(2)+1 << endl;
      }
      if (geometry->elem[iElem]->GetVTK_Type() == RECTANGLE) {
        Tecplot_File <<
        geometry->elem[iElem]->GetNode(0)+1 <<" "<< geometry->elem[iElem]->GetNode(1)+1 <<" "<<
        geometry->elem[iElem]->GetNode(2)+1 <<" "<< geometry->elem[iElem]->GetNode(3)+1 << endl;
      }
      if (geometry->elem[iElem]->GetVTK_Type() == TETRAHEDRON) {
        Tecplot_File <<
        geometry->elem[iElem]->GetNode(0)+1 <<" "<< geometry->elem[iElem]->GetNode(1)+1 <<" "<<
        geometry->elem[iElem]->GetNode(2)+1 <<" "<< geometry->elem[iElem]->GetNode(2)+1 <<" "<<
        geometry->elem[iElem]->GetNode(3)+1 <<" "<< geometry->elem[iElem]->GetNode(3)+1 <<" "<<
        geometry->elem[iElem]->GetNode(3)+1 <<" "<< geometry->elem[iElem]->GetNode(3)+1 << endl;
      }
      if (geometry->elem[iElem]->GetVTK_Type() == HEXAHEDRON) {
        Tecplot_File <<
        geometry->elem[iElem]->GetNode(0)+1 <<" "<< geometry->elem[iElem]->GetNode(1)+1 <<" "<<
        geometry->elem[iElem]->GetNode(2)+1 <<" "<< geometry->elem[iElem]->GetNode(3)+1 <<" "<<
        geometry->elem[iElem]->GetNode(4)+1 <<" "<< geometry->elem[iElem]->GetNode(5)+1 <<" "<<
        geometry->elem[iElem]->GetNode(6)+1 <<" "<< geometry->elem[iElem]->GetNode(7)+1 << endl;
      }
      if (geometry->elem[iElem]->GetVTK_Type() == PYRAMID) {
        Tecplot_File <<
        geometry->elem[iElem]->GetNode(0)+1 <<" "<< geometry->elem[iElem]->GetNode(1)+1 <<" "<<
        geometry->elem[iElem]->GetNode(2)+1 <<" "<< geometry->elem[iElem]->GetNode(3)+1 <<" "<<
        geometry->elem[iElem]->GetNode(4)+1 <<" "<< geometry->elem[iElem]->GetNode(4)+1 <<" "<<
        geometry->elem[iElem]->GetNode(4)+1 <<" "<< geometry->elem[iElem]->GetNode(4)+1 << endl;
      }
      if (geometry->elem[iElem]->GetVTK_Type() == WEDGE) {
        Tecplot_File <<
        geometry->elem[iElem]->GetNode(0)+1 <<" "<< geometry->elem[iElem]->GetNode(1)+1 <<" "<<
        geometry->elem[iElem]->GetNode(1)+1 <<" "<< geometry->elem[iElem]->GetNode(2)+1 <<" "<<
        geometry->elem[iElem]->GetNode(3)+1 <<" "<< geometry->elem[iElem]->GetNode(4)+1 <<" "<<
        geometry->elem[iElem]->GetNode(4)+1 <<" "<< geometry->elem[iElem]->GetNode(5)+1 << endl;
      }
    }
    
    Tecplot_File.close();
    
  }
  
  
#ifdef HAVE_MPI
  
  MPI_Barrier(MPI_COMM_WORLD);
  
  /*--- Add solution files to a single file ---*/
  
  if (rank == MASTER_NODE) {
    
    ofstream Tecplot_File;
    string filename, text_line;
    char buffer_char[50], out_file[MAX_STRING_SIZE];

    if (!config->GetAdjoint()) {
      if (surf_sol) filename = config->GetSurfFlowCoeff_FileName();
      else filename = config->GetFlow_FileName();
    }
    else {
      if (surf_sol) filename = config->GetSurfAdjCoeff_FileName();
      else filename = config->GetAdj_FileName();
    }
    
    filename.erase(filename.end()-2,filename.end());
    strcpy(mesh_filename, filename.c_str());
    sprintf (buffer_char, ".dat");
    strcat(mesh_filename, buffer_char);
    
    Tecplot_File.open(mesh_filename, ios::out);
    
    for (int iRank = 0; iRank < size; iRank++) {
      
      if (!config->GetAdjoint()) {
        if (surf_sol) filename = config->GetSurfFlowCoeff_FileName();
        else filename = config->GetFlow_FileName();
      }
      else {
        if (surf_sol) filename = config->GetSurfAdjCoeff_FileName();
        else filename = config->GetAdj_FileName();
      }
      
      filename.erase(filename.end()-2,filename.end());
      strcpy(out_file, filename.c_str());
      sprintf (buffer_char, "_%i.dat", iRank+1);
      strcat(out_file, buffer_char);
      ifstream Tecplot_File_;
      Tecplot_File_.open(out_file, ios::in);
      while (getline (Tecplot_File_, text_line)) {
        Tecplot_File << text_line << endl;
      }
      Tecplot_File_.close();
      remove (out_file);
    }
    
    Tecplot_File.close();
    
  }
  
#endif
  
}

void COutput::SetTecplot_ASCII(CConfig *config, CGeometry *geometry, CSolver **solver, unsigned short val_iZone, unsigned short val_nZone, bool surf_sol) {
  
  /*--- Local variables and initialization ---*/
  unsigned short iDim, iVar, nDim = geometry->GetnDim();
  unsigned short Kind_Solver = config->GetKind_Solver();
  
  unsigned long iPoint, iElem, iNode;
  unsigned long iExtIter = config->GetExtIter();
  unsigned long *LocalIndex = NULL;
  bool *SurfacePoint = NULL;
  
  bool grid_movement  = config->GetGrid_Movement();
  bool adjoint = config->GetAdjoint();

  char cstr[200], buffer[50];
  string filename;
  
  /*--- Write file name with extension ---*/
  if (surf_sol) {
    if (adjoint)
    filename = config->GetSurfAdjCoeff_FileName();
    else
    filename = config->GetSurfFlowCoeff_FileName();
  }
  else {
    if (adjoint)
    filename = config->GetAdj_FileName();
    else
    filename = config->GetFlow_FileName();
  }
  
  if (Kind_Solver == LINEAR_ELASTICITY) {
    if (surf_sol) filename = config->GetSurfStructure_FileName().c_str();
    else filename = config->GetStructure_FileName().c_str();
  }
  
  if (Kind_Solver == WAVE_EQUATION) {
    if (surf_sol) filename = config->GetSurfWave_FileName().c_str();
    else filename = config->GetWave_FileName().c_str();
  }
  
  if (Kind_Solver == HEAT_EQUATION) {
    if (surf_sol) filename = config->GetSurfHeat_FileName().c_str();
    else filename = config->GetHeat_FileName().c_str();
  }
  
  if (Kind_Solver == POISSON_EQUATION)
  filename = config->GetStructure_FileName().c_str();
  
  strcpy (cstr, filename.c_str());
  if (Kind_Solver == POISSON_EQUATION) strcpy (cstr, config->GetStructure_FileName().c_str());
  
  /*--- Special cases where a number needs to be appended to the file name. ---*/
  if ((Kind_Solver == EULER || Kind_Solver == NAVIER_STOKES || Kind_Solver == RANS) &&
      (val_nZone > 1) && (config->GetUnsteady_Simulation() != TIME_SPECTRAL)) {
    sprintf (buffer, "_%d", int(val_iZone));
    strcat(cstr,buffer);
  }
  
  /*--- Special cases where a number needs to be appended to the file name. ---*/
  if (((Kind_Solver == ADJ_EULER) || (Kind_Solver == ADJ_NAVIER_STOKES) || (Kind_Solver == ADJ_RANS)) &&
      (val_nZone > 1) && (config->GetUnsteady_Simulation() != TIME_SPECTRAL)) {
    sprintf (buffer, "_%d", int(val_iZone));
    strcat(cstr,buffer);
  }
  
  if (config->GetUnsteady_Simulation() == TIME_SPECTRAL) {
    
    /*--- SU2_SOL requires different names. It is only called for parallel cases. ---*/
    if (config->GetKind_SU2() == SU2_SOL) {
      val_iZone = iExtIter;
    }
    if (int(val_iZone) < 10) sprintf (buffer, "_0000%d.dat", int(val_iZone));
    if ((int(val_iZone) >= 10) && (int(val_iZone) < 100)) sprintf (buffer, "_000%d.dat", int(val_iZone));
    if ((int(val_iZone) >= 100) && (int(val_iZone) < 1000)) sprintf (buffer, "_00%d.dat", int(val_iZone));
    if ((int(val_iZone) >= 1000) && (int(val_iZone) < 10000)) sprintf (buffer, "_0%d.dat", int(val_iZone));
    if (int(val_iZone) >= 10000) sprintf (buffer, "_%d.dat", int(val_iZone));
    
  } else if (config->GetUnsteady_Simulation() && config->GetWrt_Unsteady()) {
    if (int(iExtIter) < 10) sprintf (buffer, "_0000%d.dat", int(iExtIter));
    if ((int(iExtIter) >= 10) && (int(iExtIter) < 100)) sprintf (buffer, "_000%d.dat", int(iExtIter));
    if ((int(iExtIter) >= 100) && (int(iExtIter) < 1000)) sprintf (buffer, "_00%d.dat", int(iExtIter));
    if ((int(iExtIter) >= 1000) && (int(iExtIter) < 10000)) sprintf (buffer, "_0%d.dat", int(iExtIter));
    if (int(iExtIter) >= 10000) sprintf (buffer, "_%d.dat", int(iExtIter));
  } else {
    sprintf (buffer, ".dat");
  }
  
  strcat(cstr,buffer);
  
  /*--- Open Tecplot ASCII file and write the header. ---*/
  ofstream Tecplot_File;
  Tecplot_File.open(cstr, ios::out);
  Tecplot_File.precision(6);
  if (surf_sol) Tecplot_File << "TITLE = \"Visualization of the surface solution\"" << endl;
  else Tecplot_File << "TITLE = \"Visualization of the volumetric solution\"" << endl;
  
  /*--- Prepare the variable lists. ---*/
  
  /*--- Write the list of the fields in the restart file.
   Without including the PointID---*/
  if (config->GetKind_SU2() == SU2_SOL) {
    
    /*--- If SU2_SOL called this routine, we already have a set of output
     variables with the appropriate string tags stored in the config class. ---*/
    Tecplot_File << "VARIABLES = ";
    nVar_Total = config->fields.size() - 1;
    for (unsigned short iField = 1; iField < config->fields.size(); iField++) {
      Tecplot_File << config->fields[iField];
    }
    Tecplot_File << endl;
    
  } else {
    
    if (nDim == 2) {
      Tecplot_File << "VARIABLES = \"x\",\"y\"";
    } else {
      Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\"";
    }
    
    /*--- Add names for conservative and residual variables ---*/
    for (iVar = 0; iVar < nVar_Consv; iVar++) {
      Tecplot_File << ",\"Conservative_" << iVar+1 << "\"";
    }
    
    if (!config->GetLow_MemoryOutput()) {
      
      if (config->GetWrt_Limiters()) {
        for (iVar = 0; iVar < nVar_Consv; iVar++) {
          Tecplot_File << ",\"Limiter_" << iVar+1 << "\"";
        }
      }
      if (config->GetWrt_Residuals()) {
        for (iVar = 0; iVar < nVar_Consv; iVar++) {
          Tecplot_File << ",\"Residual_" << iVar+1 << "\"";
        }
      }
      
      /*--- Add names for any extra variables (this will need to be adjusted). ---*/
      if (grid_movement) {
        if (nDim == 2) {
          Tecplot_File << ",\"Grid_Velx\",\"Grid_Vely\"";
        } else {
          Tecplot_File << ",\"Grid_Velx\",\"Grid_Vely\",\"Grid_Velz\"";
        }
      }
      
      if (config->GetKind_Regime() == FREESURFACE) {
        Tecplot_File << ",\"Density\"";
      }
      
      if ((Kind_Solver == EULER) || (Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
        Tecplot_File << ",\"Pressure\",\"Temperature\",\"Pressure_Coefficient\",\"Mach\"";
      }
      
      if ((Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
        Tecplot_File << ",\"Laminar_Viscosity\", \"Skin_Friction_Coefficient\", \"Heat_Flux\", \"Y_Plus\"";
      }
      
      if (Kind_Solver == RANS) {
        Tecplot_File << ", \"Eddy_Viscosity\"";
      }
      
      if (config->GetWrt_SharpEdges()) {
        if ((Kind_Solver == EULER) || (Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
          Tecplot_File << ", \"Sharp_Edge_Dist\"";
        }
      }
      
      if ((Kind_Solver == TNE2_EULER) || (Kind_Solver == TNE2_NAVIER_STOKES)) {
        Tecplot_File << ",\"Mach\",\"Pressure\",\"Temperature\",\"Temperature_ve\"";
      }
      
      if (Kind_Solver == TNE2_NAVIER_STOKES) {
        for (unsigned short iSpecies = 0; iSpecies < config->GetnSpecies(); iSpecies++)
          Tecplot_File << ",\"DiffusionCoeff_" << iSpecies << "\"";
        Tecplot_File << ",\"Laminar_Viscosity\",\"ThermConductivity\",\"ThermConductivity_ve\"";
      }
      
      if (Kind_Solver == POISSON_EQUATION) {
        unsigned short iDim;
        for (iDim = 0; iDim < geometry->GetnDim(); iDim++)
          Tecplot_File << ",\"poissonField_" << iDim+1 << "\"";
      }
      
      if (( Kind_Solver == ADJ_EULER              ) ||
          ( Kind_Solver == ADJ_NAVIER_STOKES      ) ||
          ( Kind_Solver == ADJ_RANS               ) ||
          ( Kind_Solver == ADJ_TNE2_EULER         ) ||
          ( Kind_Solver == ADJ_TNE2_NAVIER_STOKES )   ) {
        Tecplot_File << ", \"Surface_Sensitivity\", \"Solution_Sensor\"";
      }
      
      if (Kind_Solver == LINEAR_ELASTICITY) {
        Tecplot_File << ", \"Von_Mises_Stress\", \"Flow_Pressure\"";
      }
      
      if (config->GetExtraOutput()) {
        string *headings = NULL;
        //if (Kind_Solver == RANS){
        headings = solver[TURB_SOL]->OutputHeadingNames;
        //}
        for (iVar = 0; iVar < nVar_Extra; iVar++) {
          //Tecplot_File << ", \"ExtraOutput_" << iVar+1<<"\"";
          if (headings == NULL){
            Tecplot_File << ", \"ExtraOutput_" << iVar+1<<"\"";
          }else{
            Tecplot_File << ", \""<< headings[iVar] <<"\"";
          }
        }
      }
    }
    
    Tecplot_File << endl;
    
  }
  
  /*--- If it's a surface output, print only the points
   that are in the element list, change the numbering ---*/
  
  if (surf_sol) {
    
    LocalIndex = new unsigned long [nGlobal_Poin+1];
    SurfacePoint = new bool [nGlobal_Poin+1];
    
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) SurfacePoint[iPoint] = false;
    
    for(iElem = 0; iElem < nGlobal_Line; iElem++) {
      iNode = iElem*N_POINTS_LINE;
      SurfacePoint[Conn_Line[iNode+0]] = true;
      SurfacePoint[Conn_Line[iNode+1]] = true;
    }
    for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      SurfacePoint[Conn_BoundTria[iNode+0]] = true;
      SurfacePoint[Conn_BoundTria[iNode+1]] = true;
      SurfacePoint[Conn_BoundTria[iNode+2]] = true;
    }
    for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      SurfacePoint[Conn_BoundQuad[iNode+0]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+1]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+2]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+3]] = true;
    }
    
    nSurf_Poin = 0;
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
      LocalIndex[iPoint] = 0;
      if (SurfacePoint[iPoint]) { nSurf_Poin++; LocalIndex[iPoint] = nSurf_Poin; }
    }
    
  }
  
  /*--- Write the header ---*/
  Tecplot_File << "ZONE ";
  if (config->GetUnsteady_Simulation() && config->GetWrt_Unsteady()) {
    Tecplot_File << "STRANDID="<<int(iExtIter+1)<<", SOLUTIONTIME="<<config->GetDelta_UnstTime()*iExtIter<<", ";
  } else if (config->GetUnsteady_Simulation() == TIME_SPECTRAL) {
    /*--- Compute period of oscillation & compute time interval using nTimeInstances ---*/
    double period = config->GetTimeSpectral_Period();
    double deltaT = period/(double)(config->GetnTimeInstances());
    Tecplot_File << "STRANDID="<<int(iExtIter+1)<<", SOLUTIONTIME="<<deltaT*iExtIter<<", ";
  }
  
  if (nDim == 2) {
    if (surf_sol) Tecplot_File << "NODES= "<< nSurf_Poin <<", ELEMENTS= "<< nSurf_Elem <<", DATAPACKING=POINT, ZONETYPE=FELINESEG"<< endl;
    else Tecplot_File << "NODES= "<< nGlobal_Poin <<", ELEMENTS= "<< nGlobal_Elem <<", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
  } else {
    if (surf_sol) Tecplot_File << "NODES= "<< nSurf_Poin<<", ELEMENTS= "<< nSurf_Elem <<", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
    else Tecplot_File << "NODES= "<< nGlobal_Poin <<", ELEMENTS= "<< nGlobal_Elem <<", DATAPACKING=POINT, ZONETYPE=FEBRICK"<< endl;
  }
  
  /*--- Write surface and volumetric solution data. ---*/
  
  for (iPoint = 0; iPoint < nGlobal_Poin; iPoint++) {
    
    if (surf_sol) {
      
      if (LocalIndex[iPoint+1] != 0) {
        
        /*--- Write the node coordinates ---*/
        if (config->GetKind_SU2() != SU2_SOL) {
          for(iDim = 0; iDim < nDim; iDim++)
          Tecplot_File << scientific << Coords[iDim][iPoint] << "\t";
        }
        
        /*--- Loop over the vars/residuals and write the values to file ---*/
        for (iVar = 0; iVar < nVar_Total; iVar++)
        Tecplot_File << scientific << Data[iVar][iPoint] << "\t";
        
        Tecplot_File << endl;
        
      }
      
    } else {
      
      /*--- Write the node coordinates ---*/
      if (config->GetKind_SU2() != SU2_SOL) {
        for(iDim = 0; iDim < nDim; iDim++)
        Tecplot_File << scientific << Coords[iDim][iPoint] << "\t";
      }
      
      /*--- Loop over the vars/residuals and write the values to file ---*/
      for (iVar = 0; iVar < nVar_Total; iVar++)
      Tecplot_File << scientific << Data[iVar][iPoint] << "\t";
      
      Tecplot_File << endl;
      
    }
    
  }
  
  
  /*--- Write connectivity data. ---*/
  if (surf_sol) {
    
    for(iElem = 0; iElem < nGlobal_Line; iElem++) {
      iNode = iElem*N_POINTS_LINE;
      Tecplot_File << LocalIndex[Conn_Line[iNode+0]] << "\t";
      Tecplot_File << LocalIndex[Conn_Line[iNode+1]] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+0]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+1]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+2]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+2]] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+0]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+1]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+2]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+3]] << "\n";
    }
    
  } else {
    
    for(iElem = 0; iElem < nGlobal_Tria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      Tecplot_File << Conn_Tria[iNode+0] << "\t";
      Tecplot_File << Conn_Tria[iNode+1] << "\t";
      Tecplot_File << Conn_Tria[iNode+2] << "\t";
      Tecplot_File << Conn_Tria[iNode+2] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Quad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      Tecplot_File << Conn_Quad[iNode+0] << "\t";
      Tecplot_File << Conn_Quad[iNode+1] << "\t";
      Tecplot_File << Conn_Quad[iNode+2] << "\t";
      Tecplot_File << Conn_Quad[iNode+3] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Tetr; iElem++) {
      iNode = iElem*N_POINTS_TETRAHEDRON;
      Tecplot_File << Conn_Tetr[iNode+0] << "\t" << Conn_Tetr[iNode+1] << "\t";
      Tecplot_File << Conn_Tetr[iNode+2] << "\t" << Conn_Tetr[iNode+2] << "\t";
      Tecplot_File << Conn_Tetr[iNode+3] << "\t" << Conn_Tetr[iNode+3] << "\t";
      Tecplot_File << Conn_Tetr[iNode+3] << "\t" << Conn_Tetr[iNode+3] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Hexa; iElem++) {
      iNode = iElem*N_POINTS_HEXAHEDRON;
      Tecplot_File << Conn_Hexa[iNode+0] << "\t" << Conn_Hexa[iNode+1] << "\t";
      Tecplot_File << Conn_Hexa[iNode+2] << "\t" << Conn_Hexa[iNode+3] << "\t";
      Tecplot_File << Conn_Hexa[iNode+4] << "\t" << Conn_Hexa[iNode+5] << "\t";
      Tecplot_File << Conn_Hexa[iNode+6] << "\t" << Conn_Hexa[iNode+7] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Wedg; iElem++) {
      iNode = iElem*N_POINTS_WEDGE;
      Tecplot_File << Conn_Wedg[iNode+0] << "\t" << Conn_Wedg[iNode+1] << "\t";
      Tecplot_File << Conn_Wedg[iNode+1] << "\t" << Conn_Wedg[iNode+2] << "\t";
      Tecplot_File << Conn_Wedg[iNode+3] << "\t" << Conn_Wedg[iNode+4] << "\t";
      Tecplot_File << Conn_Wedg[iNode+4] << "\t" << Conn_Wedg[iNode+5] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Pyra; iElem++) {
      iNode = iElem*N_POINTS_PYRAMID;
      Tecplot_File << Conn_Pyra[iNode+0] << "\t" << Conn_Pyra[iNode+1] << "\t";
      Tecplot_File << Conn_Pyra[iNode+2] << "\t" << Conn_Pyra[iNode+3] << "\t";
      Tecplot_File << Conn_Pyra[iNode+4] << "\t" << Conn_Pyra[iNode+4] << "\t";
      Tecplot_File << Conn_Pyra[iNode+4] << "\t" << Conn_Pyra[iNode+4] << "\n";
    }
  }
  
  Tecplot_File.close();
  
  if (surf_sol) delete [] LocalIndex;
  
}

void COutput::SetTecplot_MeshASCII(CConfig *config, CGeometry *geometry, bool surf_sol, bool new_file) {
  
  unsigned short iDim, nDim = geometry->GetnDim();
  unsigned long iPoint, iElem, iNode;
  unsigned long *LocalIndex = NULL;
  bool *SurfacePoint = NULL;
  char cstr[200];
  ofstream Tecplot_File;

  if (surf_sol) strcpy(cstr, "surface_grid.dat");
  else strcpy(cstr, "volumetric_grid.dat");
  
  /*--- Open Tecplot ASCII file and write the header. ---*/
  
  if (new_file) {
    Tecplot_File.open(cstr, ios::out);
    Tecplot_File.precision(6);
    if (surf_sol) Tecplot_File << "TITLE = \"Visualization of the surface solution\"" << endl;
    else Tecplot_File << "TITLE = \"Visualization of the volumetric solution\"" << endl;
    
    if (nDim == 2) Tecplot_File << "VARIABLES = \"x\",\"y\"";
    else Tecplot_File << "VARIABLES = \"x\",\"y\",\"z\"";
  }
  else Tecplot_File.open(cstr, ios::out | ios::app);
  Tecplot_File << endl;
  
  /*--- If it's a surface output, print only the points
   that are in the element list, change the numbering ---*/
  
  if (surf_sol) {
    
    LocalIndex = new unsigned long [nGlobal_Poin+1];
    SurfacePoint = new bool [nGlobal_Poin+1];
    
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) SurfacePoint[iPoint] = false;
    
    for(iElem = 0; iElem < nGlobal_Line; iElem++) {
      iNode = iElem*N_POINTS_LINE;
      SurfacePoint[Conn_Line[iNode+0]] = true;
      SurfacePoint[Conn_Line[iNode+1]] = true;
    }
    for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      SurfacePoint[Conn_BoundTria[iNode+0]] = true;
      SurfacePoint[Conn_BoundTria[iNode+1]] = true;
      SurfacePoint[Conn_BoundTria[iNode+2]] = true;
    }
    for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      SurfacePoint[Conn_BoundQuad[iNode+0]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+1]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+2]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+3]] = true;
    }
    
    nSurf_Poin = 0;
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
      LocalIndex[iPoint] = 0;
      if (SurfacePoint[iPoint]) { nSurf_Poin++; LocalIndex[iPoint] = nSurf_Poin; }
    }
    
  }
  
  /*--- Write the header ---*/
  
  Tecplot_File << "ZONE T= ";
  if (new_file) Tecplot_File << "\"Original grid\", C=BLACK, ";
  else Tecplot_File << "\"Deformed grid\", C=RED, ";
  
  if (nDim == 2) {
    if (surf_sol) Tecplot_File << "NODES= "<< nSurf_Poin <<", ELEMENTS= "<< nSurf_Elem <<", DATAPACKING=POINT, ZONETYPE=FELINESEG"<< endl;
    else Tecplot_File << "NODES= "<< nGlobal_Poin <<", ELEMENTS= "<< nGlobal_Elem <<", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
  } else {
    if (surf_sol) Tecplot_File << "NODES= "<< nSurf_Poin<<", ELEMENTS= "<< nSurf_Elem <<", DATAPACKING=POINT, ZONETYPE=FEQUADRILATERAL"<< endl;
    else Tecplot_File << "NODES= "<< nGlobal_Poin <<", ELEMENTS= "<< nGlobal_Elem <<", DATAPACKING=POINT, ZONETYPE=FEBRICK"<< endl;
  }
  
  /*--- Write surface and volumetric solution data. ---*/
  
  for (iPoint = 0; iPoint < nGlobal_Poin; iPoint++) {
    
    if (surf_sol) {
      
      if (LocalIndex[iPoint+1] != 0) {
        
        /*--- Write the node coordinates ---*/
          for(iDim = 0; iDim < nDim; iDim++)
            Tecplot_File << scientific << Coords[iDim][iPoint] << "\t";
        
        Tecplot_File << endl;
        
      }
      
    } else {
      
      /*--- Write the node coordinates ---*/

      for(iDim = 0; iDim < nDim; iDim++)
          Tecplot_File << scientific << Coords[iDim][iPoint] << "\t";
      
      
      Tecplot_File << endl;
      
    }
    
  }
  
  
  /*--- Write connectivity data. ---*/
  
  if (surf_sol) {
    
    for(iElem = 0; iElem < nGlobal_Line; iElem++) {
      iNode = iElem*N_POINTS_LINE;
      Tecplot_File << LocalIndex[Conn_Line[iNode+0]] << "\t";
      Tecplot_File << LocalIndex[Conn_Line[iNode+1]] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+0]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+1]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+2]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundTria[iNode+2]] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+0]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+1]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+2]] << "\t";
      Tecplot_File << LocalIndex[Conn_BoundQuad[iNode+3]] << "\n";
    }
    
  } else {
    
    for(iElem = 0; iElem < nGlobal_Tria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      Tecplot_File << Conn_Tria[iNode+0] << "\t";
      Tecplot_File << Conn_Tria[iNode+1] << "\t";
      Tecplot_File << Conn_Tria[iNode+2] << "\t";
      Tecplot_File << Conn_Tria[iNode+2] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Quad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      Tecplot_File << Conn_Quad[iNode+0] << "\t";
      Tecplot_File << Conn_Quad[iNode+1] << "\t";
      Tecplot_File << Conn_Quad[iNode+2] << "\t";
      Tecplot_File << Conn_Quad[iNode+3] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Tetr; iElem++) {
      iNode = iElem*N_POINTS_TETRAHEDRON;
      Tecplot_File << Conn_Tetr[iNode+0] << "\t" << Conn_Tetr[iNode+1] << "\t";
      Tecplot_File << Conn_Tetr[iNode+2] << "\t" << Conn_Tetr[iNode+2] << "\t";
      Tecplot_File << Conn_Tetr[iNode+3] << "\t" << Conn_Tetr[iNode+3] << "\t";
      Tecplot_File << Conn_Tetr[iNode+3] << "\t" << Conn_Tetr[iNode+3] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Hexa; iElem++) {
      iNode = iElem*N_POINTS_HEXAHEDRON;
      Tecplot_File << Conn_Hexa[iNode+0] << "\t" << Conn_Hexa[iNode+1] << "\t";
      Tecplot_File << Conn_Hexa[iNode+2] << "\t" << Conn_Hexa[iNode+3] << "\t";
      Tecplot_File << Conn_Hexa[iNode+4] << "\t" << Conn_Hexa[iNode+5] << "\t";
      Tecplot_File << Conn_Hexa[iNode+6] << "\t" << Conn_Hexa[iNode+7] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Wedg; iElem++) {
      iNode = iElem*N_POINTS_WEDGE;
      Tecplot_File << Conn_Wedg[iNode+0] << "\t" << Conn_Wedg[iNode+1] << "\t";
      Tecplot_File << Conn_Wedg[iNode+1] << "\t" << Conn_Wedg[iNode+2] << "\t";
      Tecplot_File << Conn_Wedg[iNode+3] << "\t" << Conn_Wedg[iNode+4] << "\t";
      Tecplot_File << Conn_Wedg[iNode+4] << "\t" << Conn_Wedg[iNode+5] << "\n";
    }
    
    for(iElem = 0; iElem < nGlobal_Pyra; iElem++) {
      iNode = iElem*N_POINTS_PYRAMID;
      Tecplot_File << Conn_Pyra[iNode+0] << "\t" << Conn_Pyra[iNode+1] << "\t";
      Tecplot_File << Conn_Pyra[iNode+2] << "\t" << Conn_Pyra[iNode+3] << "\t";
      Tecplot_File << Conn_Pyra[iNode+4] << "\t" << Conn_Pyra[iNode+4] << "\t";
      Tecplot_File << Conn_Pyra[iNode+4] << "\t" << Conn_Pyra[iNode+4] << "\n";
    }
  }
  
  Tecplot_File.close();
  
  if (surf_sol) delete [] LocalIndex;
  
}

void COutput::SetSU2_MeshASCII(CConfig *config, CGeometry *geometry) {
  
  ofstream SU2_File;
  char cstr[MAX_STRING_SIZE], out_file[MAX_STRING_SIZE], in_file[MAX_STRING_SIZE];
  string str;
  unsigned long iElem, iPoint, iElem_Bound, nElem_, nElem_Bound_, vnodes_edge[2], vnodes_triangle[3], vnodes_quad[4], vnodes_tetra[4], vnodes_hexa[8], vnodes_wedge[6], vnodes_pyramid[5];
  unsigned short iMarker, iDim, nDim = geometry->GetnDim(), iChar, iPeriodic, nPeriodic = 0, VTK_Type, nDim_, nMarker_;
  double *center, *angles, *transl;
  ofstream output_file;
  ifstream input_file;
  string Grid_Marker, text_line, Marker_Tag;
  string::size_type position;

  /*--- Read the name of the output and input file ---*/
  
  str = config->GetMesh_Out_FileName();
  strcpy (out_file, str.c_str());
  strcpy (cstr, out_file);
  output_file.precision(15);
  output_file.open(cstr, ios::out);

  str = config->GetMesh_FileName();
  strcpy (in_file, str.c_str());
  strcpy (cstr, in_file);
  input_file.open(cstr, ios::out);
  
  /*--- Read grid file with format SU2 ---*/
  
  while (getline (input_file, text_line)) {
    
    /*--- Read the dimension of the problem ---*/
    
    position = text_line.find ("NDIME=",0);
    if (position != string::npos) {
      text_line.erase (0,6); nDim_ = atoi(text_line.c_str());
      output_file << "NDIME= " << nDim_ << endl;
    }
    
    /*--- Read the information about inner elements ---*/
    
    position = text_line.find ("NELEM=",0);
    if (position != string::npos) {
      text_line.erase (0,6); nElem_ = atoi(text_line.c_str());
      output_file << "NELEM= " << nElem_ << endl;
      
      /*--- Loop over all the volumetric elements ---*/
      
      for (iElem = 0; iElem < nElem_;  iElem++) {
        getline(input_file, text_line);
        istringstream elem_line(text_line);
        
        elem_line >> VTK_Type;
        output_file << VTK_Type;
        
        switch(VTK_Type) {
          case TRIANGLE:
            elem_line >> vnodes_triangle[0]; elem_line >> vnodes_triangle[1]; elem_line >> vnodes_triangle[2];
            output_file << "\t" << vnodes_triangle[0] << "\t" << vnodes_triangle[1] << "\t" << vnodes_triangle[2] << endl;
            break;
          case RECTANGLE:
            elem_line >> vnodes_quad[0]; elem_line >> vnodes_quad[1]; elem_line >> vnodes_quad[2]; elem_line >> vnodes_quad[3];
            output_file << "\t" << vnodes_quad[0] << "\t" << vnodes_quad[1] << "\t" << vnodes_quad[2] << "\t" << vnodes_quad[3] << endl;
            break;
          case TETRAHEDRON:
            elem_line >> vnodes_tetra[0]; elem_line >> vnodes_tetra[1]; elem_line >> vnodes_tetra[2]; elem_line >> vnodes_tetra[3];
            output_file << "\t" << vnodes_tetra[0] << "\t" << vnodes_tetra[1] << "\t" << vnodes_tetra[2] << "\t" << vnodes_tetra[3] << endl;
            break;
          case HEXAHEDRON:
            elem_line >> vnodes_hexa[0]; elem_line >> vnodes_hexa[1]; elem_line >> vnodes_hexa[2];
            elem_line >> vnodes_hexa[3]; elem_line >> vnodes_hexa[4]; elem_line >> vnodes_hexa[5];
            elem_line >> vnodes_hexa[6]; elem_line >> vnodes_hexa[7];
            output_file << "\t" << vnodes_hexa[0] << "\t" << vnodes_hexa[1] << "\t" << vnodes_hexa[2] << "\t" << vnodes_hexa[3] << "\t" << vnodes_hexa[4] << "\t" << vnodes_hexa[5] << "\t" << vnodes_hexa[6] << "\t" << vnodes_hexa[7] << endl;
            break;
          case WEDGE:
            elem_line >> vnodes_wedge[0]; elem_line >> vnodes_wedge[1]; elem_line >> vnodes_wedge[2];
            elem_line >> vnodes_wedge[3]; elem_line >> vnodes_wedge[4]; elem_line >> vnodes_wedge[5];
            output_file << "\t" << vnodes_wedge[0] << "\t" << vnodes_wedge[1] << "\t" << vnodes_wedge[2] << "\t" << vnodes_wedge[3] << "\t" << vnodes_wedge[4] << "\t" << vnodes_wedge[5] << endl;
            break;
          case PYRAMID:
            elem_line >> vnodes_pyramid[0]; elem_line >> vnodes_pyramid[1]; elem_line >> vnodes_pyramid[2];
            elem_line >> vnodes_pyramid[3]; elem_line >> vnodes_pyramid[4];
            output_file << "\t" << vnodes_pyramid[0] << "\t" << vnodes_pyramid[1] << "\t" << vnodes_pyramid[2] << "\t" << vnodes_pyramid[3] << "\t" << vnodes_pyramid[4] << endl;
            break;
        }
      }
    }
    
    /*--- Coordinates ---*/
    
    position = text_line.find ("NPOIN=",0);
    if (position != string::npos) {

      /*--- Skip the lines about the points ---*/
      
      for (iPoint = 0; iPoint < nGlobal_Poin;  iPoint++) {
        getline(input_file, text_line);
      }
      
      /*--- Add the new coordinates ---*/
      
      output_file << "NPOIN= " << nGlobal_Poin << endl;
      
      /*--- Write surface and volumetric solution data. ---*/
      
      for (iPoint = 0; iPoint < nGlobal_Poin; iPoint++) {
        
        /*--- Write the node coordinates ---*/
        
        for(iDim = 0; iDim < nDim; iDim++)
          output_file << scientific << Coords[iDim][iPoint] << "\t";
        
        output_file << iPoint << endl;
        
      }
      
    }
    
    /*--- Write the physical boundaries ---*/
    
    position = text_line.find ("NMARK=",0);
    if (position != string::npos) {
      
      text_line.erase (0,6); nMarker_ = atoi(text_line.c_str());
      output_file << "NMARK= " << nMarker_ << endl;
      
      for (iMarker = 0 ; iMarker < nMarker_; iMarker++) {
        
        getline (input_file,text_line);
        text_line.erase (0,11);
        string::size_type position;
        for (iChar = 0; iChar < 20; iChar++) {
          position = text_line.find( " ", 0 );
          if(position != string::npos) text_line.erase (position,1);
          position = text_line.find( "\r", 0 );
          if(position != string::npos) text_line.erase (position,1);
          position = text_line.find( "\n", 0 );
          if(position != string::npos) text_line.erase (position,1);
        }
        Marker_Tag = text_line.c_str();
        
        /*--- Standart physical boundary ---*/
        
          getline (input_file, text_line);
          
          text_line.erase (0,13); nElem_Bound_ = atoi(text_line.c_str());
          output_file << "MARKER_TAG= " << Marker_Tag << endl;
          output_file << "MARKER_ELEMS= " << nElem_Bound_<< endl;
          
          for (iElem_Bound = 0; iElem_Bound < nElem_Bound_; iElem_Bound++) {
            
            getline(input_file, text_line);
            istringstream bound_line(text_line);
            
            bound_line >> VTK_Type;
            output_file << VTK_Type;
            
            switch(VTK_Type) {
              case LINE:
                bound_line >> vnodes_edge[0]; bound_line >> vnodes_edge[1];
                output_file << "\t" << vnodes_edge[0] << "\t" << vnodes_edge[1] << endl;
                break;
              case TRIANGLE:
                bound_line >> vnodes_triangle[0]; bound_line >> vnodes_triangle[1]; bound_line >> vnodes_triangle[2];
                output_file << "\t" << vnodes_triangle[0] << "\t" << vnodes_triangle[1] << "\t" << vnodes_triangle[2] << endl;
                break;
              case RECTANGLE:
                bound_line >> vnodes_quad[0]; bound_line >> vnodes_quad[1]; bound_line >> vnodes_quad[2]; bound_line >> vnodes_quad[3];
                output_file << "\t" << vnodes_quad[0] << "\t" << vnodes_quad[1] << "\t" << vnodes_quad[2] << "\t" << vnodes_quad[3] << endl;
                break;
            }
          }
      }
    }
  }
  
  
  /*--- Get the total number of periodic transformations ---*/
  
  nPeriodic = config->GetnPeriodicIndex();
  output_file << "NPERIODIC= " << nPeriodic << endl;
  
  /*--- From iPeriodic obtain the iMarker ---*/
  
  for (iPeriodic = 0; iPeriodic < nPeriodic; iPeriodic++) {
    
    /*--- Retrieve the supplied periodic information. ---*/
    
    center = config->GetPeriodicCenter(iPeriodic);
    angles = config->GetPeriodicRotation(iPeriodic);
    transl = config->GetPeriodicTranslate(iPeriodic);
    
    output_file << "PERIODIC_INDEX= " << iPeriodic << endl;
    output_file << center[0] << "\t" << center[1] << "\t" << center[2] << endl;
    output_file << angles[0] << "\t" << angles[1] << "\t" << angles[2] << endl;
    output_file << transl[0] << "\t" << transl[1] << "\t" << transl[2] << endl;
    
  }
  
  input_file.close();
  output_file.close();
  
}

void COutput::SetTecplot_MeshBinary(CConfig *config, CGeometry *geometry, unsigned short val_iZone) {
  
#ifdef HAVE_TECIO
  
  double   t;
  INTEGER4 i, N, err, Debug, NPts, NElm, IsDouble, KMax;
  INTEGER4 ICellMax, JCellMax, KCellMax, ZoneType, StrandID, ParentZn, FileType;
  INTEGER4 *ShareFromZone, IsBlock, NumFaceConnections, FaceNeighborMode, ShareConnectivityFromZone;
  string buffer, variables;
  stringstream file;
  bool first_zone = true;
  unsigned short dims = geometry->GetnDim();
  enum     FileType { FULL = 0, GRID = 1, SOLUTION = 2 };
  enum	 ZoneType { ORDERED=0, FELINESEG=1, FETRIANGLE=2, FEQUADRILATERAL=3, FETETRAHEDRON=4, FEBRICK=5, FEPOLYGON=6, FEPOLYHEDRON=7 };
  
  /*--- Consistent data for Tecplot zones ---*/
  
  Debug						= 0;
  IsDouble					= 1;
  NPts						= (INTEGER4)nGlobal_Poin;
  t							= 0.0;//iExtIter*config->GetDelta_UnstTimeND();
  KMax						= 0;
  ICellMax					= 0;
  JCellMax					= 0;
  KCellMax					= 0;
  StrandID					= 0;//(INTEGER4)iExtIter;
  ParentZn					= 0;
  IsBlock						= 1;
  NumFaceConnections			= 0;
  FaceNeighborMode			= 0;
  ShareConnectivityFromZone	= 0;
  
  /*--- Write Tecplot solution file ---*/
  
  if (!wrote_base_file) {
    
    file.str(string());
    buffer = config->GetFlow_FileName();

#ifdef HAVE_MPI
	/*--- Remove the domain number from the filename ---*/
    
    int nProcessor;
	MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
    if (nProcessor > 1) buffer.erase(buffer.end()-2, buffer.end());
#endif

    file << buffer << ".mesh.plt";
    FileType = GRID;
    
    if (dims == 2) variables = "x y";
    else if (dims == 3) variables = "x y z";
    else cout << "Error: wrong number of dimensions: " << dims << endl;
    
    /*--- Open Tecplot file ---*/
    
    err = TECINI112((char *)config->GetFlow_FileName().c_str(),
                    (char *)variables.c_str(),
                    (char *)file.str().c_str(),
                    (char *)".",
                    &FileType,
                    &Debug,
                    &IsDouble);
    if (err) cout << "Error in opening Tecplot file" << endl;
    
    first_zone = true;
    ShareFromZone = new INTEGER4[dims];
    for (i = 0; i < dims; i++) ShareFromZone[i] = 0;
    
    if (nGlobal_Tria > 0) {
      
      /*--- Write the zone header information ---*/
      ZoneType = FETRIANGLE; NElm = (INTEGER4)nGlobal_Tria; N = NElm*N_POINTS_TRIANGLE;
      
      err = TECZNE112((char*)"Triangle Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      ShareFromZone,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      
      if (first_zone) {
        
        if (config->GetKind_SU2() == SU2_SOL) {
          err = TECDAT112(&NPts, Data[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Data[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Data[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        } else {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_Tria);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
    }
    if (nGlobal_Quad > 0) {
      
      /*--- Write the zone header information ---*/
      
      ZoneType = FEQUADRILATERAL; NElm = (INTEGER4)nGlobal_Quad; N = NElm*N_POINTS_QUADRILATERAL;
      
      err = TECZNE112((char*)"Quadrilateral Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      ShareFromZone,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      
      if (first_zone) {
        
        if (config->GetKind_SU2() == SU2_SOL) {
          err = TECDAT112(&NPts, Data[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Data[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Data[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }else {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_Quad);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
    }
    if (nGlobal_Tetr > 0) {
      
      /*--- Write the zone header information ---*/
      
      ZoneType = FETETRAHEDRON; NElm = (INTEGER4)nGlobal_Tetr; N = NElm*N_POINTS_TETRAHEDRON;
      
      err = TECZNE112((char*)"Tetrahedral Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      NULL,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      
      if (first_zone) {
        
        if (config->GetKind_SU2() == SU2_SOL) {
          err = TECDAT112(&NPts, Data[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Data[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Data[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }else {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_Tetr);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
    }
    if (nGlobal_Hexa > 0) {
      
      /*--- Write the zone header information ---*/
      
      ZoneType = FEBRICK; NElm = (INTEGER4)nGlobal_Hexa; N = NElm*N_POINTS_HEXAHEDRON;
      
      err = TECZNE112((char*)"Hexahedral Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      NULL,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      
      if (first_zone) {
        
        if (config->GetKind_SU2() == SU2_SOL) {
          err = TECDAT112(&NPts, Data[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Data[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Data[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }else {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[0] = 1;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[1] = 1;
          if (geometry->GetnDim() == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            ShareFromZone[2] = 1;
          }
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_Hexa);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
    }
    if (nGlobal_Pyra > 0) {
      cout << "Pyramid element type not yet supported; no zone written." << endl;
    }
    if (nGlobal_Wedg > 0) {
      cout << "Wedge element type not yet supported; no zone written." << endl;
    }
    
    delete [] ShareFromZone;
    wrote_base_file = true;
    
    err = TECEND112();
    if (err) cout << "Error in closing Tecplot file" << endl;
    
  }
  
#endif
  
}

void COutput::SetTecplot_SurfaceMesh(CConfig *config, CGeometry *geometry, unsigned short val_iZone) {
  
#ifdef HAVE_TECIO
  
  double   t;
  INTEGER4 i, N, err, Debug, NPts, NElm, IsDouble, KMax;
  INTEGER4 ICellMax, JCellMax, KCellMax, ZoneType, StrandID, ParentZn, FileType;
  INTEGER4 *ShareFromZone, IsBlock, NumFaceConnections, FaceNeighborMode, ShareConnectivityFromZone;
  string buffer, variables;
  stringstream file;
  bool first_zone = true;
  unsigned short iDim, dims = geometry->GetnDim();
  unsigned long iPoint, iElem, iNode;
  enum     FileType { FULL = 0, GRID = 1, SOLUTION = 2 };
  enum	 ZoneType { ORDERED=0, FELINESEG=1, FETRIANGLE=2, FEQUADRILATERAL=3, FETETRAHEDRON=4, FEBRICK=5, FEPOLYGON=6, FEPOLYHEDRON=7 };
  
  
  /*--- Write Tecplot solution file ---*/
  if (!wrote_surf_file) {
    
    file.str(string());
    buffer = config->GetSurfFlowCoeff_FileName();
    
#ifdef HAVE_MPI
	/*--- Remove the domain number from the filename ---*/
    int nProcessor;
	MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
    if (nProcessor > 1) buffer.erase(buffer.end()-2, buffer.end());
#endif
    
    file << buffer << ".mesh.plt";
    FileType = GRID;
    
    if (dims == 2) variables = "x y";
    else if (dims == 3) variables = "x y z";
    else cout << "Error: wrong number of dimensions: " << dims << endl;
    
    first_zone = true;
    ShareFromZone = new INTEGER4[dims];
    for (i = 0; i < dims; i++) ShareFromZone[i] = 0;
    
    /*--- Perform a renumbering for the surface points/elements ---*/
    unsigned long *LocalIndex = new unsigned long [nGlobal_Poin+1];
    bool *SurfacePoint = new bool [nGlobal_Poin+1];
    
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) SurfacePoint[iPoint] = false;
    
    for(iElem = 0; iElem < nGlobal_Line; iElem++) {
      iNode = iElem*N_POINTS_LINE;
      SurfacePoint[Conn_Line[iNode+0]] = true;
      SurfacePoint[Conn_Line[iNode+1]] = true;
    }
    for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
      iNode = iElem*N_POINTS_TRIANGLE;
      SurfacePoint[Conn_BoundTria[iNode+0]] = true;
      SurfacePoint[Conn_BoundTria[iNode+1]] = true;
      SurfacePoint[Conn_BoundTria[iNode+2]] = true;
    }
    for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
      iNode = iElem*N_POINTS_QUADRILATERAL;
      SurfacePoint[Conn_BoundQuad[iNode+0]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+1]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+2]] = true;
      SurfacePoint[Conn_BoundQuad[iNode+3]] = true;
    }
    
    unsigned long nSurf_Poin = 0;
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
      LocalIndex[iPoint] = 0;
      if (SurfacePoint[iPoint]) { nSurf_Poin++; LocalIndex[iPoint] = nSurf_Poin; }
    }
    
    /*--- Collect surface coordinates into one array as well ---*/
    /*--- Note the -1 in the Coords/Data array in order to undo the 1-based indexing ---*/
    double **Surf_Coords = new double*[dims];
    for (iDim = 0; iDim < dims; iDim++)
    Surf_Coords[iDim] = new double[nSurf_Poin];
    
    unsigned long iSurf_Poin = 0;
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
      if (SurfacePoint[iPoint]) {
        for (iDim = 0; iDim < dims; iDim++) {
          if (config->GetKind_SU2() == SU2_SOL)
          Surf_Coords[iDim][iSurf_Poin] = Data[iDim][iPoint-1];
          else
          Surf_Coords[iDim][iSurf_Poin] = Coords[iDim][iPoint-1];
        }
        iSurf_Poin++;
      }
    }
    
    /*--- Consistent data for Tecplot zones ---*/
    Debug						= 0;
    IsDouble					= 1;
    NPts						= (INTEGER4)nSurf_Poin;
    t							= 0.0;//iExtIter*config->GetDelta_UnstTimeND();
    KMax						= 0;
    ICellMax					= 0;
    JCellMax					= 0;
    KCellMax					= 0;
    StrandID					= 0;//(INTEGER4)iExtIter;
    ParentZn					= 0;
    IsBlock						= 1;
    NumFaceConnections			= 0;
    FaceNeighborMode			= 0;
    ShareConnectivityFromZone	= 0;
    
    /*--- Open Tecplot file ---*/
    err = TECINI112((char *)config->GetFlow_FileName().c_str(),
                    (char *)variables.c_str(),
                    (char *)file.str().c_str(),
                    (char *)".",
                    &FileType,
                    &Debug,
                    &IsDouble);
    if (err) cout << "Error in opening Tecplot file" << endl;
    
    
    if (nGlobal_Line > 0) {
      
      /*--- Put the connectivity into a single array for writing ---*/
      int *Conn_Line_New = new int[nGlobal_Line*N_POINTS_LINE];
      iNode = 0;
      for(iElem = 0; iElem < nGlobal_Line; iElem++) {
        iNode = iElem*N_POINTS_LINE;
        Conn_Line_New[iNode+0] = LocalIndex[Conn_Line[iNode+0]];
        Conn_Line_New[iNode+1] = LocalIndex[Conn_Line[iNode+1]];
      }
      
      /*--- Write the zone header information ---*/
      ZoneType = FELINESEG; NElm = (INTEGER4)nGlobal_Line; N = NElm*N_POINTS_LINE;
      
      err = TECZNE112((char*)"Line Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      NULL,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      if (first_zone) {
        
        err = TECDAT112(&NPts, Surf_Coords[0], &IsDouble); ShareFromZone[0] = 1;
        err = TECDAT112(&NPts, Surf_Coords[1], &IsDouble); ShareFromZone[1] = 1;
        if (geometry->GetnDim() == 3) {
          err = TECDAT112(&NPts, Surf_Coords[2], &IsDouble);
          ShareFromZone[2] = 1;
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_Line_New);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
      delete [] Conn_Line_New;
    }
    
    if (nGlobal_BoundTria > 0) {
      
      /*--- Put the connectivity into a single array for writing ---*/
      int *Conn_BoundTria_New = new int[nGlobal_BoundTria*N_POINTS_TRIANGLE];
      
      iNode = 0;
      for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
        iNode = iElem*N_POINTS_TRIANGLE;
        Conn_BoundTria_New[iNode+0] = LocalIndex[Conn_BoundTria[iNode+0]];
        Conn_BoundTria_New[iNode+1] = LocalIndex[Conn_BoundTria[iNode+1]];
        Conn_BoundTria_New[iNode+2] = LocalIndex[Conn_BoundTria[iNode+2]];
      }
      
      /*--- Write the zone header information ---*/
      ZoneType = FETRIANGLE; NElm = (INTEGER4)nGlobal_BoundTria; N = NElm*N_POINTS_TRIANGLE;
      
      err = TECZNE112((char*)"Triangle Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      ShareFromZone,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      if (first_zone) {
        
        err = TECDAT112(&NPts, Surf_Coords[0], &IsDouble); ShareFromZone[0] = 1;
        err = TECDAT112(&NPts, Surf_Coords[1], &IsDouble); ShareFromZone[1] = 1;
        if (geometry->GetnDim() == 3) {
          err = TECDAT112(&NPts, Surf_Coords[2], &IsDouble);
          ShareFromZone[2] = 1;
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_BoundTria_New);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
      delete [] Conn_BoundTria_New;
    }
    
    if (nGlobal_BoundQuad > 0) {
      
      
      /*--- Put the connectivity into a single array for writing ---*/
      int *Conn_BoundQuad_New = new int[nGlobal_BoundQuad*N_POINTS_QUADRILATERAL];
      iNode = 0;
      for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
        iNode = iElem*N_POINTS_QUADRILATERAL;
        Conn_BoundQuad_New[iNode+0] = LocalIndex[Conn_BoundQuad[iNode+0]];
        Conn_BoundQuad_New[iNode+1] = LocalIndex[Conn_BoundQuad[iNode+1]];
        Conn_BoundQuad_New[iNode+2] = LocalIndex[Conn_BoundQuad[iNode+2]];
        Conn_BoundQuad_New[iNode+3] = LocalIndex[Conn_BoundQuad[iNode+3]];
      }
      
      /*--- Write the zone header information ---*/
      ZoneType = FEQUADRILATERAL; NElm = (INTEGER4)nGlobal_BoundQuad; N = NElm*N_POINTS_QUADRILATERAL;
      
      err = TECZNE112((char*)"Quadrilateral Elements",
                      &ZoneType,
                      &NPts,
                      &NElm,
                      &KMax,
                      &ICellMax,
                      &JCellMax,
                      &KCellMax,
                      &t,
                      &StrandID,
                      &ParentZn,
                      &IsBlock,
                      &NumFaceConnections,
                      &FaceNeighborMode,
                      0,         /* TotalNumFaceNodes */
                      0,         /* NumConnectedBoundaryFaces */
                      0,         /* TotalNumBoundaryConnections */
                      NULL,      /* PassiveVarList */
                      NULL,      /* ValueLocation */
                      ShareFromZone,      /* ShareVarFromZone */
                      &ShareConnectivityFromZone);
      if (err) cout << "Error writing Tecplot zone data" << endl;
      
      /*--- write node coordinates and data if not done already---*/
      if (first_zone) {
        
        err = TECDAT112(&NPts, Surf_Coords[0], &IsDouble); ShareFromZone[0] = 1;
        err = TECDAT112(&NPts, Surf_Coords[1], &IsDouble); ShareFromZone[1] = 1;
        if (geometry->GetnDim() == 3) {
          err = TECDAT112(&NPts, Surf_Coords[2], &IsDouble);
          ShareFromZone[2] = 1;
        }
        if (err) cout << "Error writing coordinates to Tecplot file" << endl;
        first_zone = false;
      }
      
      err = TECNOD112(Conn_BoundQuad_New);
      if (err) cout << "Error writing connectivity to Tecplot file" << endl;
      
      delete [] Conn_BoundQuad_New;
    }
    
    for (iDim = 0; iDim < dims; iDim++)
    delete [] Surf_Coords[iDim];
    delete [] Surf_Coords;
    delete [] ShareFromZone;
    delete [] LocalIndex;
    delete [] SurfacePoint;
    wrote_surf_file = true;
    
    err = TECEND112();
    if (err) cout << "Error in closing Tecplot file" << endl;
    
  }
  
#endif
  
}

void COutput::SetTecplot_Solution(CConfig *config, CGeometry *geometry, unsigned short val_iZone) {
  
#ifdef HAVE_TECIO
  
  double   t;
  INTEGER4 i, N, iVar, err, Debug, NPts, NElm, IsDouble, KMax;
  INTEGER4 ICellMax, JCellMax, KCellMax, ZoneType, StrandID, ParentZn, FileType;
  INTEGER4 *ShareFromZone, IsBlock, NumFaceConnections, FaceNeighborMode, ShareConnectivityFromZone;
  string buffer, variables;
  stringstream file;
  bool first_zone = true, unsteady = config->GetUnsteady_Simulation(), GridMovement = config->GetGrid_Movement();
  bool Wrt_Unsteady = config->GetWrt_Unsteady();
  unsigned long iExtIter = config->GetExtIter();
  unsigned short NVar, dims = geometry->GetnDim();
  enum     FileType { FULL = 0, GRID = 1, SOLUTION = 2 };
  enum	 ZoneType { ORDERED=0, FELINESEG=1, FETRIANGLE=2, FEQUADRILATERAL=3, FETETRAHEDRON=4, FEBRICK=5, FEPOLYGON=6, FEPOLYHEDRON=7 };
  
  /*--- Consistent data for Tecplot zones ---*/
  Debug						= 0;
  IsDouble					= 1;
  NPts						= (INTEGER4)nGlobal_Poin;
  t							= iExtIter*config->GetDelta_UnstTimeND();
  KMax						= 0;
  ICellMax					= 0;
  JCellMax					= 0;
  KCellMax					= 0;
  StrandID					= (INTEGER4)iExtIter+1;
  ParentZn					= 0;
  IsBlock						= 1;
  NumFaceConnections			= 0;
  FaceNeighborMode			= 0;
  ShareConnectivityFromZone	= 0;
  
  
  file.str(string());
  buffer = config->GetFlow_FileName();

#ifdef HAVE_MPI
	/*--- Remove the domain number from the filename ---*/
    int nProcessor;
	MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
    if (nProcessor > 1) buffer.erase(buffer.end()-2, buffer.end());
#endif
  
  file << buffer;
  
  if (unsteady) {
    if (((int)iExtIter >= 0) && ((int)iExtIter < 10))			file << "_0000" << iExtIter;
    if (((int)iExtIter >= 10) && ((int)iExtIter < 100))		file << "_000" << iExtIter;
    if (((int)iExtIter >= 100) && ((int)iExtIter < 1000))		file << "_00" << iExtIter;
    if (((int)iExtIter >= 1000) && ((int)iExtIter < 10000))	file << "_0" << iExtIter;
    if ((int)iExtIter >= 10000)							file << iExtIter;
  }
  file << ".sol.plt";
  FileType = SOLUTION;
  variables = AssembleVariableNames(geometry, config, nVar_Consv, &NVar);
  if (config->GetKind_SU2() == SU2_SOL) {
    if (Wrt_Unsteady && GridMovement) nVar_Total = NVar;
    else nVar_Total = NVar+dims;
  }
  
  /*--- Open Tecplot file ---*/
  err = TECINI112((char *)config->GetFlow_FileName().c_str(),
                  (char *)variables.c_str(),
                  (char *)file.str().c_str(),
                  (char *)".",
                  &FileType,
                  &Debug,
                  &IsDouble);
  if (err) cout << "Error in opening Tecplot file" << endl;
  
  first_zone = true;
  ShareFromZone = new INTEGER4[NVar];
  for (i = 0; i < NVar; i++) ShareFromZone[i] = 0;
  
  if (nGlobal_Tria > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FETRIANGLE; NElm = (INTEGER4)nGlobal_Tria; N = NElm*N_POINTS_TRIANGLE;
    
    err = TECZNE112((char*)"Triangle Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    ShareFromZone,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iVar = 0; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        } else {
          for (iVar = dims; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          if (dims == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            if (err) cout << "Error writing coordinates to Tecplot file" << endl;
            ShareFromZone[i++] = 1;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      
      first_zone = false;
    }
    
  }
  if (nGlobal_Quad > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FEQUADRILATERAL; NElm = (INTEGER4)nGlobal_Quad; N = NElm*N_POINTS_QUADRILATERAL;
    
    err = TECZNE112((char*)"Quadrilateral Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    ShareFromZone,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iVar = 0; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        } else {
          for (iVar = dims; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          if (dims == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            if (err) cout << "Error writing coordinates to Tecplot file" << endl;
            ShareFromZone[i++] = 1;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      
      first_zone = false;
    }
    
  }
  if (nGlobal_Tetr > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FETETRAHEDRON; NElm = (INTEGER4)nGlobal_Tetr; N = NElm*N_POINTS_TETRAHEDRON;
    
    err = TECZNE112((char*)"Tetrahedral Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    NULL,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iVar = 0; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        } else {
          for (iVar = dims; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          if (dims == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            if (err) cout << "Error writing coordinates to Tecplot file" << endl;
            ShareFromZone[i++] = 1;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      
      first_zone = false;
    }
    
  }
  if (nGlobal_Hexa > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FEBRICK; NElm = (INTEGER4)nGlobal_Hexa; N = NElm*N_POINTS_HEXAHEDRON;
    
    err = TECZNE112((char*)"Hexahedral Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    NULL,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iVar = 0; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        } else {
          for (iVar = dims; iVar < nVar_Total; iVar++) {
            err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          err = TECDAT112(&NPts, Coords[0], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          err = TECDAT112(&NPts, Coords[1], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing coordinates to Tecplot file" << endl;
          if (dims == 3) {
            err = TECDAT112(&NPts, Coords[2], &IsDouble);
            if (err) cout << "Error writing coordinates to Tecplot file" << endl;
            ShareFromZone[i++] = 1;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      
      first_zone = false;
    }
    
  }
  if (nGlobal_Pyra > 0) {
    cout << "Pyramid element type not yet supported; no zone written." << endl;
  }
  if (nGlobal_Wedg > 0) {
    cout << "Wedge element type not yet supported; no zone written." << endl;
  }
  
  delete [] ShareFromZone;
  
  err = TECEND112();
  if (err) cout << "Error in closing Tecplot file" << endl;
  
#endif
  
}

void COutput::SetTecplot_SurfaceSolution(CConfig *config, CGeometry *geometry, unsigned short val_iZone) {
  
#ifdef HAVE_TECIO
  
  double   t;
  INTEGER4 i, N, iVar, err, Debug, NPts, NElm, IsDouble, KMax;
  INTEGER4 ICellMax, JCellMax, KCellMax, ZoneType, StrandID, ParentZn, FileType;
  INTEGER4 *ShareFromZone, IsBlock, NumFaceConnections, FaceNeighborMode, ShareConnectivityFromZone;
  string buffer, variables;
  stringstream file;
  bool first_zone = true, unsteady = config->GetUnsteady_Simulation(), GridMovement = config->GetGrid_Movement();
  bool Wrt_Unsteady = config->GetWrt_Unsteady();
  unsigned long iPoint, iElem, iNode, iSurf_Poin, iExtIter = config->GetExtIter();
  unsigned short iDim, NVar, dims = geometry->GetnDim();
  enum     FileType { FULL = 0, GRID = 1, SOLUTION = 2 };
  enum	 ZoneType { ORDERED=0, FELINESEG=1, FETRIANGLE=2, FEQUADRILATERAL=3, FETETRAHEDRON=4, FEBRICK=5, FEPOLYGON=6, FEPOLYHEDRON=7 };
  
  
  file.str(string());
  buffer = config->GetSurfFlowCoeff_FileName();

#ifdef HAVE_MPI
	/*--- Remove the domain number from the filename ---*/
    int nProcessor;
	MPI_Comm_size(MPI_COMM_WORLD, &nProcessor);
    if (nProcessor > 1) buffer.erase(buffer.end()-2, buffer.end());
#endif
  
  file << buffer;
  
  if (unsteady) {
    if (((int)iExtIter >= 0) && ((int)iExtIter < 10))			file << "_0000" << iExtIter;
    if (((int)iExtIter >= 10) && ((int)iExtIter < 100))		file << "_000" << iExtIter;
    if (((int)iExtIter >= 100) && ((int)iExtIter < 1000))		file << "_00" << iExtIter;
    if (((int)iExtIter >= 1000) && ((int)iExtIter < 10000))	file << "_0" << iExtIter;
    if ((int)iExtIter >= 10000)							file << iExtIter;
  }
  file << ".sol.plt";
  FileType = SOLUTION;
  variables = AssembleVariableNames(geometry, config, nVar_Consv, &NVar);
  if (config->GetKind_SU2() == SU2_SOL) {
    if (Wrt_Unsteady && GridMovement) nVar_Total = NVar;
    else nVar_Total = NVar+dims;
  }
  
  first_zone = true;
  ShareFromZone = new INTEGER4[NVar];
  for (i = 0; i < NVar; i++) ShareFromZone[i] = 0;
  
  
  /*--- Perform a renumbering for the surface points/elements ---*/
  unsigned long *LocalIndex = new unsigned long [nGlobal_Poin+1];
  bool *SurfacePoint = new bool [nGlobal_Poin+1];
  
  for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) SurfacePoint[iPoint] = false;
  
  for(iElem = 0; iElem < nGlobal_Line; iElem++) {
    iNode = iElem*N_POINTS_LINE;
    SurfacePoint[Conn_Line[iNode+0]] = true;
    SurfacePoint[Conn_Line[iNode+1]] = true;
  }
  for(iElem = 0; iElem < nGlobal_BoundTria; iElem++) {
    iNode = iElem*N_POINTS_TRIANGLE;
    SurfacePoint[Conn_BoundTria[iNode+0]] = true;
    SurfacePoint[Conn_BoundTria[iNode+1]] = true;
    SurfacePoint[Conn_BoundTria[iNode+2]] = true;
  }
  for(iElem = 0; iElem < nGlobal_BoundQuad; iElem++) {
    iNode = iElem*N_POINTS_QUADRILATERAL;
    SurfacePoint[Conn_BoundQuad[iNode+0]] = true;
    SurfacePoint[Conn_BoundQuad[iNode+1]] = true;
    SurfacePoint[Conn_BoundQuad[iNode+2]] = true;
    SurfacePoint[Conn_BoundQuad[iNode+3]] = true;
  }
  
  unsigned long nSurf_Poin = 0;
  for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
    LocalIndex[iPoint] = 0;
    if (SurfacePoint[iPoint]) { nSurf_Poin++; LocalIndex[iPoint] = nSurf_Poin; }
  }
  
  /*--- Collect surface coordinates into one array as well ---*/
  /*--- Note the -1 in the Coords/Data array in order to undo the 1-based indexing ---*/
  double **Surf_Coords;
  if (Wrt_Unsteady && GridMovement) {
    Surf_Coords = new double*[dims];
    for (iDim = 0; iDim < dims; iDim++)
    Surf_Coords[iDim] = new double[nSurf_Poin];
    
    iSurf_Poin = 0;
    for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
      if (SurfacePoint[iPoint]) {
        for (iDim = 0; iDim < dims; iDim++) {
          if (config->GetKind_SU2() == SU2_SOL)
          Surf_Coords[iDim][iSurf_Poin] = Data[iDim][iPoint-1];
          else
          Surf_Coords[iDim][iSurf_Poin] = Coords[iDim][iPoint-1];
        }
        iSurf_Poin++;
      }
    }
  }
  
  /*--- Collect surface data into one array for the surface as well ---*/
  /*--- Note the -1 in the Coords/Data array in order to undo the 1-based indexing ---*/
  double **Surf_Data = new double*[nVar_Total];
  for (iVar = 0; iVar < nVar_Total; iVar++)
  Surf_Data[iVar] = new double[nSurf_Poin];
  
  iSurf_Poin = 0;
  for (iPoint = 0; iPoint < nGlobal_Poin+1; iPoint++) {
    if (SurfacePoint[iPoint]) {
      for (iVar = 0; iVar < nVar_Total; iVar++) {
        if (config->GetKind_SU2() == SU2_SOL) {
          if (Wrt_Unsteady && GridMovement)
          Surf_Data[iVar][iSurf_Poin] = Data[iVar+dims][iPoint-1];
          else
          Surf_Data[iVar][iSurf_Poin] = Data[iVar][iPoint-1];
        } else
        Surf_Data[iVar][iSurf_Poin] = Data[iVar][iPoint-1];
      }
      iSurf_Poin++;
    }
  }
  
  /*--- Consistent data for Tecplot zones ---*/
  Debug						= 0;
  IsDouble					= 1;
  NPts						= (INTEGER4)nSurf_Poin;
  t							= iExtIter*config->GetDelta_UnstTimeND();
  KMax						= 0;
  ICellMax					= 0;
  JCellMax					= 0;
  KCellMax					= 0;
  StrandID					= (INTEGER4)iExtIter+1;
  ParentZn					= 0;
  IsBlock						= 1;
  NumFaceConnections			= 0;
  FaceNeighborMode			= 0;
  ShareConnectivityFromZone	= 0;
  
  
  /*--- Open Tecplot file ---*/
  err = TECINI112((char *)config->GetFlow_FileName().c_str(),
                  (char *)variables.c_str(),
                  (char *)file.str().c_str(),
                  (char *)".",
                  &FileType,
                  &Debug,
                  &IsDouble);
  if (err) cout << "Error in opening Tecplot file" << endl;
  
  
  if (nGlobal_Line > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FELINESEG; NElm = (INTEGER4)nGlobal_Line; N = NElm*N_POINTS_LINE;
    
    err = TECZNE112((char*)"Line Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    ShareFromZone,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iDim = 0; iDim < dims; iDim++) {
            err = TECDAT112(&NPts, Surf_Data[iDim], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
        for (iVar = dims; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Surf_Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          for (iDim = 0; iDim < dims; iDim++) {
            err = TECDAT112(&NPts, Surf_Coords[iDim], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Surf_Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      first_zone = false;
    }
    
  }
  
  if (nGlobal_BoundTria > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FETRIANGLE; NElm = (INTEGER4)nGlobal_BoundTria; N = NElm*N_POINTS_TRIANGLE;
    
    err = TECZNE112((char*)"Triangle Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    ShareFromZone,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iDim = 0; iDim < dims; iDim++) {
            err = TECDAT112(&NPts, Surf_Data[iDim], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
        for (iVar = dims; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Surf_Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          for (iDim = 0; iDim < dims; iDim++) {
            err = TECDAT112(&NPts, Surf_Coords[iDim], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Surf_Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      first_zone = false;
    }
    
  }
  
  if (nGlobal_BoundQuad > 0) {
    
    /*--- Write the zone header information ---*/
    ZoneType = FEQUADRILATERAL; NElm = (INTEGER4)nGlobal_BoundQuad; N = NElm*N_POINTS_QUADRILATERAL;
    
    err = TECZNE112((char*)"Quadrilateral Elements",
                    &ZoneType,
                    &NPts,
                    &NElm,
                    &KMax,
                    &ICellMax,
                    &JCellMax,
                    &KCellMax,
                    &t,
                    &StrandID,
                    &ParentZn,
                    &IsBlock,
                    &NumFaceConnections,
                    &FaceNeighborMode,
                    0,         /* TotalNumFaceNodes */
                    0,         /* NumConnectedBoundaryFaces */
                    0,         /* TotalNumBoundaryConnections */
                    NULL,      /* PassiveVarList */
                    NULL,      /* ValueLocation */
                    ShareFromZone,      /* ShareVarFromZone */
                    &ShareConnectivityFromZone);
    if (err) cout << "Error writing Tecplot zone data" << endl;
    
    /*--- write node coordinates and data if not done already---*/
    if (first_zone) {
      
      i = 0;
      if (config->GetKind_SU2() == SU2_SOL) {
        if (Wrt_Unsteady && GridMovement) {
          for (iDim = 0; iDim < dims; iDim++) {
            err = TECDAT112(&NPts, Surf_Data[iDim], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
        for (iVar = dims; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Surf_Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      } else {
        if (Wrt_Unsteady && GridMovement) {
          for (iDim = 0; iDim < dims; iDim++) {
            err = TECDAT112(&NPts, Surf_Coords[iDim], &IsDouble); ShareFromZone[i++] = 1;
            if (err) cout << "Error writing data to Tecplot file" << endl;
          }
        }
        for (iVar = 0; iVar < nVar_Total; iVar++) {
          err = TECDAT112(&NPts, Surf_Data[iVar], &IsDouble); ShareFromZone[i++] = 1;
          if (err) cout << "Error writing data to Tecplot file" << endl;
        }
      }
      first_zone = false;
    }
    
  }
  
  for (iVar = 0; iVar < nVar_Total; iVar++)
  delete [] Surf_Data[iVar];
  delete [] Surf_Data;
  
  if (Wrt_Unsteady && GridMovement) {
    for (iDim = 0; iDim < dims; iDim++)
    delete [] Surf_Coords[iDim];
    delete [] Surf_Coords;
  }
  delete [] LocalIndex;
  delete [] SurfacePoint;
  delete [] ShareFromZone;
  
  err = TECEND112();
  if (err) cout << "Error in closing Tecplot file" << endl;
  
#endif
  
}

string AssembleVariableNames(CGeometry *geometry, CConfig *config, unsigned short nVar_Consv, unsigned short *NVar) {
  
  /*--- Local variables ---*/
  stringstream variables; variables.str(string());
  unsigned short iVar;
  *NVar = 0;
  unsigned short iDim, nDim = geometry->GetnDim();
  unsigned short Kind_Solver  = config->GetKind_Solver();
  bool grid_movement = config->GetGrid_Movement();
  bool Wrt_Unsteady = config->GetWrt_Unsteady();
  
  
  /*--- Write the basic variable header based on the particular solution ----*/
  
  /*--- Write the list of the fields in the restart file.
   Without including the PointID---*/
  if (config->GetKind_SU2() == SU2_SOL) {
    
    /*--- If SU2_SOL called this routine, we already have a set of output
     variables with the appropriate string tags stored in the config class.
     We simply read in and remove the quotation marks from the var names. ---*/
    
    /*--- Set the number of variables to be written. Subtract off an index for
     the PointID as well as each coordinate (x,y,z). ---*/
    string varname;
    
    if (Wrt_Unsteady && grid_movement) {
      
      *NVar = config->fields.size()-1;
      for (unsigned short iField = 1; iField < config->fields.size(); iField++) {
        varname = config->fields[iField];
        varname.erase (varname.begin(), varname.begin()+1);
        varname.erase (varname.end()-1, varname.end());
        variables << varname << " ";
      }
    } else {
      
      *NVar = config->fields.size()-1-nDim;
      for (unsigned short iField = 1+nDim; iField < config->fields.size(); iField++) {
        varname = config->fields[iField];
        varname.erase (varname.begin(), varname.begin()+1);
        varname.erase (varname.end()-1, varname.end());
        variables << varname << " ";
      }
    }
    
  } else {
    
    if (Wrt_Unsteady && grid_movement) {
      if (nDim == 2) {
        variables << "x y "; *NVar += 2;
      } else {
        variables << "x y z "; *NVar += 3;
      }
    }
    
    for (iVar = 0; iVar < nVar_Consv; iVar++) {
      variables << "Conservative_" << iVar+1<<" "; *NVar += 1;
    }
    if (config->GetWrt_Limiters()) {
      for (iVar = 0; iVar < nVar_Consv; iVar++) {
        variables << "Limiter_" << iVar+1<<" "; *NVar += 1;
      }
    }
    if (config->GetWrt_Residuals()) {
      for (iVar = 0; iVar < nVar_Consv; iVar++) {
        variables << "Residual_" << iVar+1<<" "; *NVar += 1;
      }
    }
    
    /*--- Add names for any extra variables (this will need to be adjusted). ---*/
    if (grid_movement) {
      if (nDim == 2) {
        variables << "Grid_Velx Grid_Vely "; *NVar += 2;
      } else {
        variables << "Grid_Velx Grid_Vely Grid_Velz "; *NVar += 3;
      }
    }
    
    if (config->GetKind_Regime() == FREESURFACE) {
      variables << "Density ";
      *NVar += 1;
    }
    
    if ((Kind_Solver == EULER) || (Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
      variables << "Pressure Temperature Pressure_Coefficient Mach ";
      *NVar += 4;
    }
    
    if ((Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
      variables << "Laminar_Viscosity Skin_Friction_Coefficient Heat_Flux Y_Plus ";
      *NVar += 4;
    }
    
    if (Kind_Solver == RANS) {
      variables << "Eddy_Viscosity ";
      *NVar += 1;
    }
    
    if (config->GetWrt_SharpEdges()) {
      if ((Kind_Solver == EULER) || (Kind_Solver == NAVIER_STOKES) || (Kind_Solver == RANS)) {
        variables << "Sharp_Edge_Dist ";
        *NVar += 1;
      }
    }
    
    if ((Kind_Solver == TNE2_EULER) || (Kind_Solver == TNE2_NAVIER_STOKES)) {
      variables << "Mach Pressure Temperature Temperature_ve ";
      *NVar += 4;
    }
    
    if (Kind_Solver == TNE2_NAVIER_STOKES) {
      for (iVar = 0; iVar < config->GetnSpecies(); iVar++)
        variables << "DiffusionCoeff_" << iVar << " ";
      variables << "Laminar_Viscosity ThermConductivity ThermConductivity_ve";
      *NVar += 4;
    }
    
    if (Kind_Solver == POISSON_EQUATION) {
      for (iDim = 0; iDim < geometry->GetnDim(); iDim++) {
        variables << "poissonField_" << iDim+1 << " ";
        *NVar += 1;
      }
    }
    
    if (( Kind_Solver == ADJ_EULER              ) ||
        ( Kind_Solver == ADJ_NAVIER_STOKES      ) ||
        ( Kind_Solver == ADJ_RANS               ) ||
        ( Kind_Solver == ADJ_TNE2_EULER         ) ||
        ( Kind_Solver == ADJ_TNE2_NAVIER_STOKES )   ) {
      variables << "Surface_Sensitivity Solution_Sensor ";
      *NVar += 2;
    }
  }
  
  return variables.str();
  
}
