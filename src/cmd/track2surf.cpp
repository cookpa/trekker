#include "cmd.h"

using namespace NIBR;

namespace CMDARGS_TRACK2SURFACE {

    std::string inp_fname;
    std::string inp_surface;
    std::string out_surface;

    std::string feature;
    std::string field_name;

    int numberOfThreads     =  0;
    std::string verbose     = "info";
    bool force              = false;

}

using namespace CMDARGS_TRACK2SURFACE;

void run_track2surf()
{

    parseCommon(numberOfThreads,verbose);
    if (!parseForceOutput(out_surface,force)) return;

    if ((feature!="streamlineDensity") && (feature!="streamlineCount") && (feature!="contactAngle") && (feature!="contactDirection")) {
        std::cout << "Feature can be \"streamlineDensity\", \"streamlineCount\", \"contactAngle\" and \"contactDirection\"" << std::endl << std::flush;
        return;
    }
    
    // Initialize tractogram
    if(!ensureVTKorTCK(inp_fname)) return;
    NIBR::TractogramReader* tractogram = new NIBR::TractogramReader();
    tractogram->initReader(inp_fname);
    
    if (tractogram->numberOfStreamlines<1) {
        std::cout << "Empty tractogram" << std::endl;
        std::cout << "0 streamlines are written."  << std::endl << std::flush;
        return;
    }
    
    // Read surface mesh
    if(!ensureVTK(out_surface)) return;

    NIBR::Surface* surf = new NIBR::Surface();
    surf->readHeader(inp_surface);

    if ((surf->extension != "vtk") && (surf->extension != "gii")) {
        disp(MSG_ERROR,"Unsupported file format: %s (only vtk/gii are supported)", inp_surface.c_str());
        return;
    }
    
    // Check if this field already exists in the input surface mesh
    int fieldId = -1;
    std::vector<NIBR::SurfaceField> tmpFields = surf->findFields();
    for (size_t i=0; i<tmpFields.size(); i++) {
        if ((tmpFields[i].name == ("vertex_" + field_name)) ||  (tmpFields[i].name == ("face_" + field_name)) ) {
            fieldId = i;
        }
    }
    
    if ((force==false) && (fieldId>-1)) {
        std::cout << "A field with name \"" << field_name << "\" already exists in the input surface mesh. Use --force or -f to overwrite." << std::endl<< std::flush;
        return;
    }
    
    surf->readMesh();
    surf->readFields();
    surf->deleteField("vertex_" + field_name);
    surf->deleteField("face_"   + field_name);

    surf->calcNormalsOfFaces();
    surf->calcCentersOfFaces();
    surf->calcTriangleVectors();
    surf->getNeighboringFaces();

    // Create tractogram to surface map
    std::vector<std::vector<streamline2faceMap>> mapping;    
    tractogram2surfaceMapper(tractogram, surf, mapping, true);
    delete tractogram;

    // Prepare and write selected feature on surface
    int**   idata;
    float** fdata;
    
    float** fdata_vertex;
    float*  segmentDirPerFace;   // Magnitude of total segment directions per face, which is used for normalization
    

    if (feature=="streamlineDensity") {
        
        fdata = new float*[surf->nf];
        for (int n=0; n<surf->nf; n++) {
            fdata[n]    = new float[1];
            fdata[n][0] = 0;
        }
        
        auto compileStreamlineDensity = [&](NIBR::MT::TASK task)->void {

            fdata[task.no][0] = mapping[task.no].size()/surf->areasOfFaces[task.no];
             
        };
        NIBR::MT::MTRUN(surf->nf, "Compiling face output", compileStreamlineDensity);
        
        NIBR::SurfaceField field;
        field.name      = "face_" + field_name;;
        field.owner     = NIBR::FACE;
        field.datatype  = "float";
        field.fdata     = fdata;
        field.idata     = NULL;
        field.dimension = 1;
        surf->fields.push_back(field);
        
        
        fdata_vertex = new float*[surf->nv];
        for (int n=0; n<surf->nv; n++) {
            fdata_vertex[n]    = new float[1];
            fdata_vertex[n][0] = 0;
        }
        
        auto compileStreamlineDensityAtVertices = [&](NIBR::MT::TASK task)->void {
    
            fdata_vertex[task.no][0] = 0;
            float totArea = 0;
            
            for (auto nInd : surf->neighboringFaces[task.no]) {
                fdata_vertex[task.no][0] += mapping[nInd].size();
                totArea                  += surf->areasOfFaces[nInd];
            }
            
            if (totArea>0)
                fdata_vertex[task.no][0] /= totArea;
                
        };
        NIBR::MT::MTRUN(surf->nv, "Compiling vertex output", compileStreamlineDensityAtVertices);
        
        NIBR::SurfaceField vfield;
        vfield.name      = "vertex_" + field_name;
        vfield.owner     = NIBR::VERTEX;
        vfield.datatype  = "float";
        vfield.fdata     = fdata_vertex;
        vfield.idata     = NULL;
        vfield.dimension = 1;
        surf->fields.push_back(vfield);
        
        
    }




    if (feature=="streamlineCount") {
        
        idata = new int*[surf->nf];
        for (int n=0; n<surf->nf; n++) {
            idata[n]    = new int[1];
            idata[n][0] = 0;
        }
        
        
        auto compileStreamlineCount = [&](NIBR::MT::TASK task)->void {
            idata[task.no][0] = mapping[task.no].size();
        };
        
        NIBR::MT::MTRUN(surf->nf, "Compiling face output", compileStreamlineCount);
        
        NIBR::SurfaceField field;
        field.name      = "face_" + field_name;;
        field.owner     = NIBR::FACE;
        field.datatype  = "int";
        field.fdata     = NULL;
        field.idata     = idata;
        field.dimension = 1;
        surf->fields.push_back(field);
        
        
        fdata_vertex = new float*[surf->nv];
        for (int n=0; n<surf->nv; n++) {
            fdata_vertex[n]    = new float[1];
            fdata_vertex[n][0] = 0;
        }
        
        auto compileStreamlineCountAtVertices = [&](NIBR::MT::TASK task)->void {
    
            fdata_vertex[task.no][0] = 0;
            
            for (auto nInd : surf->neighboringFaces[task.no])
                fdata_vertex[task.no][0] += mapping[nInd].size();
            
            if (surf->neighboringFaces[task.no].size()>0)
                fdata_vertex[task.no][0] /= float(surf->neighboringFaces[task.no].size());
                
        };
        NIBR::MT::MTRUN(surf->nv, "Compiling vertex output", compileStreamlineCountAtVertices);
        
        NIBR::SurfaceField vfield;
        vfield.name      = "vertex_" + field_name;
        vfield.owner     = NIBR::VERTEX;
        vfield.datatype  = "float";
        vfield.fdata     = fdata_vertex;
        vfield.idata     = NULL;
        vfield.dimension = 1;
        surf->fields.push_back(vfield);
        
        
    }
    
    if (feature=="contactAngle") {
        
        fdata = new float*[surf->nf];
        for (int n=0; n<surf->nf; n++) {
            fdata[n]    = new float[1];
            fdata[n][0] = 0;
        }
        
        auto compileContactAngle = [&](NIBR::MT::TASK task)->void {
                
            float totalAngle = 0;

            for (auto indAng : mapping[task.no])
                totalAngle += indAng.angle;
            
            if (mapping[task.no].size()>0)
                fdata[task.no][0] = totalAngle/(float)mapping[task.no].size();

        };

        NIBR::MT::MTRUN(surf->nf, "Compiling face output", compileContactAngle);
        
        NIBR::SurfaceField field;
        field.name      = "face_" + field_name;
        field.owner     = NIBR::FACE;
        field.datatype  = "float";
        field.fdata     = fdata;
        field.idata     = NULL;
        field.dimension = 1;
        
        surf->fields.push_back(field);
        
        
        fdata_vertex = new float*[surf->nv];
        for (int n=0; n<surf->nv; n++) {
            fdata_vertex[n]    = new float[1];
            fdata_vertex[n][0] = 0;
        }
        
        auto compileContactAngleAtVertices = [&](NIBR::MT::TASK task)->void {

            fdata_vertex[task.no][0] = 0;
            
            int totalSegmentCount = 0;
            
            for (auto i : surf->neighboringFaces[task.no]) {
                fdata_vertex[task.no][0] += fdata[i][0]*float(mapping[i].size());
                totalSegmentCount        += mapping[i].size();
            }
            
            if (totalSegmentCount>0)
                fdata_vertex[task.no][0] /= float(totalSegmentCount);
            
        };
        NIBR::MT::MTRUN(surf->nv, "Compiling vertex output", compileContactAngleAtVertices);
        
        NIBR::SurfaceField vfield;
        vfield.name      = "vertex_" + field_name;
        vfield.owner     = NIBR::VERTEX;
        vfield.datatype  = "float";
        vfield.fdata     = fdata_vertex;
        vfield.idata     = NULL;
        vfield.dimension = 1;
        surf->fields.push_back(vfield);

        
    }
    
    if (feature=="contactDirection") {
        
        fdata               = new float*[surf->nf];
        segmentDirPerFace   = new float[surf->nf];
        for (int n=0; n<surf->nf; n++) {
            fdata[n]    = new float[3];
            fdata[n][0] = 0;
            fdata[n][1] = 0;
            fdata[n][2] = 0;
            segmentDirPerFace[n]   = 0;
        }
        
        fdata               = new float*[surf->nf];
        for (int n=0; n<surf->nf; n++) {
            fdata[n]    = new float[3];
            fdata[n][0] = 0;
            fdata[n][1] = 0;
            fdata[n][2] = 0;
        }
        
        auto compileContactDirection = [&](NIBR::MT::TASK task)->void {
                
            for (auto indAng : mapping[task.no]) {
                fdata[task.no][0] += std::fabs(indAng.dir[0]);
                fdata[task.no][1] += std::fabs(indAng.dir[1]);
                fdata[task.no][2] += std::fabs(indAng.dir[2]);
            }

            fdata[task.no][0] /= surf->areasOfFaces[task.no];
            fdata[task.no][1] /= surf->areasOfFaces[task.no];
            fdata[task.no][2] /= surf->areasOfFaces[task.no];
            
        if (mapping[task.no].size()>0) {
                segmentDirPerFace[task.no] = norm(fdata[task.no]);
            }
            
        };
        NIBR::MT::MTRUN(surf->nf, "Compiling face output", compileContactDirection);
        
        
        float maxDirMag = 0;
        for (int n=0; n<surf->nf; n++) {
            if (segmentDirPerFace[n]>maxDirMag)
                maxDirMag = segmentDirPerFace[n];
        }
        
        for (int n=0; n<surf->nf; n++) {
            fdata[n][0] /= maxDirMag;
            fdata[n][1] /= maxDirMag;
            fdata[n][2] /= maxDirMag;
        }
        
        NIBR::SurfaceField ffield;
        ffield.name      = "face_" + field_name;
        ffield.owner     = NIBR::FACE;
        ffield.datatype  = "float";
        ffield.fdata     = fdata;
        ffield.idata     = NULL;
        ffield.dimension = 3;
        surf->fields.push_back(ffield);
        
        
        fdata_vertex = new float*[surf->nv];
        for (int n=0; n<surf->nv; n++) {
            fdata_vertex[n]    = new float[3];
            fdata_vertex[n][0] = 0;
            fdata_vertex[n][1] = 0;
            fdata_vertex[n][2] = 0;
        }
        
        auto compileContactDirectionAtVertices = [&](NIBR::MT::TASK task)->void {
                
            for (int i=0;i<3;i++)
                fdata_vertex[task.no][i] = 0;
            
            float totArea = 0;
            
            for (auto f : surf->neighboringFaces[task.no]) {
                for (int i=0;i<3;i++)
                    fdata_vertex[task.no][i] += fdata[f][i]*surf->areasOfFaces[f];
                totArea  += surf->areasOfFaces[f];
            }
            
            if (totArea>0)
                for (int i=0;i<3;i++)
                    fdata_vertex[task.no][i] /= totArea;
                
        };
        NIBR::MT::MTRUN(surf->nv, "Compiling vertex output", compileContactDirectionAtVertices);
        
        
        NIBR::SurfaceField vfield;
        vfield.name      = "vertex_" + field_name;
        vfield.owner     = NIBR::VERTEX;
        vfield.datatype  = "float";
        vfield.fdata     = fdata_vertex;
        vfield.idata     = NULL;
        vfield.dimension = 3;
        surf->fields.push_back(vfield);
        
        
    }

    
    // Write output
    surf->write(out_surface);
    delete surf;

}


void track2surf(CLI::App* app)
{

    app->description("maps tractogram features on a surface");

    app->add_option("<input tractogram>", inp_fname, "Input tractogram (.vtk, .tck)")
        ->required()
        ->check(CLI::ExistingFile);
    
    app->add_option("<input surface mesh>", inp_surface, "Input surface mesh (.vtk,.gii)")
        ->required()
        ->check(CLI::ExistingFile);

    app->add_option("<output surface mesh>", out_surface, "Output surface mesh (.vtk)")
        ->required();
        
    app->add_option("<field name>", field_name, "Field name to use when writing the feature on the surface mesh")
        ->required();
        
    app->add_option("--feature", feature, "Name of output feature. Options are: \"streamlineDensity\", \"streamlineCount\", \"contactAngle\" and \"contactDirection\".")
        ->required();

    app->add_option("--numberOfThreads, -n", numberOfThreads,    "Number of threads.");
    app->add_option("--verbose, -v",         verbose,            "Verbose level. Options are \"quite\",\"fatal\",\"error\",\"warn\",\"info\" and \"debug\". Default=info");
    app->add_flag("--force, -f",             force,              "Force overwriting of existing file");

    app->callback(run_track2surf);

}
