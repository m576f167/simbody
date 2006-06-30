/* Portions copyright (c) 2006 Stanford University and Michael Sherman.
 * Contributors:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "simbody/internal/common.h"
#include "simbody/internal/MultibodySystem.h"
#include "simbody/internal/MatterSubsystem.h"
#include "simbody/internal/DecorativeGeometry.h"
#include "simbody/internal/VTKReporter.h"

#include "vtkCommand.h"
#include "vtkRenderer.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkProperty.h"
#include "vtkAssembly.h"
#include "vtkCamera.h"
#include "vtkLight.h"
#include "vtkActor.h"
#include "vtkFollower.h"
#include "vtkTransform.h"
#include "vtkTransformPolyDataFilter.h"

#include "vtkPolyDataMapper.h"
#include "vtkCaptionActor2D.h"
#include "vtkAppendPolyData.h"
#include "vtkInteractorObserver.h"
#include "vtkInteractorStyleTrackballCamera.h"

#include "windows.h" // kludge

#include <cassert>
#include <cmath>
#include <cstdio>
#include <exception>
#include <vector>

namespace SimTK {
static const Real Pi = std::acos(-1.), RadiansPerDegree = Pi/180;
static const int  GroundBodyNum = 0; // ground is always body 0
static const Vec3 DefaultGroundBodyColor = Green;
static const Vec3 DefaultBaseBodyColor   = Red;
static const Vec3 DefaultBodyColor       = Gray;

class VTKReporterRep {
public:
    // no default constructor -- must have MultibodySystem always
    VTKReporterRep(const MultibodySystem& m, bool generateDefaultGeometry=true);

    ~VTKReporterRep() {
        deletePointers();
    }

    void disableDefaultGeometry() { defaultGeometryEnabled=false;}

    // This will make a copy of the supplied DecorativeGeometry.
    void addDecoration(int bodyNum, const Transform& X_GD, const DecorativeGeometry&);
    void addRubberBandLine(int b1, const Vec3& station1, int b2, const Vec3& station2,
                           const DecorativeLine&);

    // Make sure everything can be seen.
    void setCameraDefault();

    void setDefaultBodyColor(int bodyNum, const Vec3& rgb) {
        bodies[bodyNum].defaultColorRGB = rgb;
    }
    const Vec3& getDefaultBodyColor(int body) const {return bodies[body].defaultColorRGB;}
    
    void setBodyScale(int bodyNum, const Real& scale) {
        bodies[bodyNum].scale = scale;
    }

    VTKReporterRep* clone() const {
        VTKReporterRep* dup = new VTKReporterRep(*this);
        dup->myHandle = 0;
        return dup;
    }

    void report(const State& s);

    void setMyHandle(VTKReporter& h) {myHandle = &h;}
    void clearMyHandle() {myHandle=0;}

private:
    friend class VTKReporter;
    VTKReporter* myHandle;     // the owner of this rep

    bool defaultGeometryEnabled;

    const MultibodySystem& mbs;

    struct PerBodyInfo {
        PerBodyInfo() : defaultColorRGB(Black), scale(1) { }
        std::vector<vtkProp3D*>         aList;
        std::vector<DecorativeGeometry> gList; // one per actor (TODO)
        Vec3        defaultColorRGB;
        Real        scale;  // overall size of body, default 1
    };
    std::vector<PerBodyInfo> bodies;

    struct PerDynamicGeomInfo {
        PerDynamicGeomInfo() : actor(0), body1(-1), body2(-1) { }
        vtkActor*      actor;
        DecorativeLine line;
        int  body1, body2;
        Vec3 station1, station2;
    };
    std::vector<PerDynamicGeomInfo> dynamicGeom;

    vtkRenderWindow* renWin;
    vtkRenderer*     renderer;

    void zeroPointers();
    void deletePointers();
    void setConfiguration(int bodyNum, const Transform& X_GB);
    void setRubberBandLine(int dgeom, const Vec3& p1, const Vec3& p2);
};

    /////////////////
    // VTKReporter //
    /////////////////


bool VTKReporter::isOwnerHandle() const {
    return rep==0 || rep->myHandle==this;
}
bool VTKReporter::isEmptyHandle() const {return rep==0;}

VTKReporter::VTKReporter(const MultibodySystem& m, bool generateDefaultGeometry) : rep(0) {
    rep = new VTKReporterRep(m, generateDefaultGeometry);
    rep->setMyHandle(*this);
}

VTKReporter::VTKReporter(const VTKReporter& src) : rep(0) {
    if (src.rep) {
        rep = src.rep->clone();
        rep->setMyHandle(*this);
    }
}

VTKReporter& VTKReporter::operator=(const VTKReporter& src) {
    if (&src != this) {
        delete rep; rep=0;
        if (src.rep) {
            rep = src.rep->clone();
            rep->setMyHandle(*this);
        }
    }
    return *this;
}

VTKReporter::~VTKReporter() {
    delete rep; rep=0;
}

void VTKReporter::report(const State& s) {
    assert(rep);
    rep->report(s);
}

void VTKReporter::addDecoration(int body, const Transform& X_GD,
                                const DecorativeGeometry& g) 
{
    assert(rep);
    rep->addDecoration(body, X_GD, g);
}

void VTKReporter::addRubberBandLine(int b1, const Vec3& station1,
                                    int b2, const Vec3& station2,
                                    const DecorativeLine& g)
{
    assert(rep);
    rep->addRubberBandLine(b1,station1,b2,station2,g);
}

void VTKReporter::setDefaultBodyColor(int bodyNum, const Vec3& rgb) {
   assert(rep);
   rep->setDefaultBodyColor(bodyNum,rgb);
}



    ////////////////////
    // VTKReporterRep //
    ////////////////////

void VTKReporterRep::addDecoration(int body, const Transform& X_GD,
                                   const DecorativeGeometry& g)
{
    // For now we create a unique actor for each piece of geometry
    vtkActor* actor = vtkActor::New();
    bodies[body].aList.push_back(actor);
    bodies[body].gList.push_back(g);
    DecorativeGeometry& geom  = bodies[body].gList.back();

    // Apply the transformation.
    geom.setPlacement(X_GD*geom.getPlacement());
    vtkPolyData* poly = geom.updVTKPolyData();

    // Now apply the actor-level properties from the geometry.
    const Vec3 color = (geom.getColor()[0] != -1 ? geom.getColor() : getDefaultBodyColor(body)); 
    actor->GetProperty()->SetColor(color[0],color[1],color[2]);

    const Real opacity = (geom.getOpacity() != -1 ? geom.getOpacity() : Real(1));
    actor->GetProperty()->SetOpacity(opacity);

    const Real lineWidth = (geom.getLineThickness() != -1 ? geom.getLineThickness() : Real(1));
    actor->GetProperty()->SetLineWidth(lineWidth);

    const int representation = (geom.getRepresentation() != -1 ? geom.getRepresentation() : VTK_SURFACE);
    actor->GetProperty()->SetRepresentation(representation);

    // Set up the mapper & register actor with renderer
    vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
    mapper->SetInput(poly);
    actor->SetMapper(mapper);
    mapper->Delete(); mapper=0; // remove now-unneeded mapper reference
    renderer->AddActor(actor);
    setCameraDefault();
}

void VTKReporterRep::addRubberBandLine(int b1, const Vec3& station1,
                                       int b2, const Vec3& station2,
                                       const DecorativeLine& g)
{
    // Create a unique actor for each piece of geometry.
    int nxt = (int)dynamicGeom.size();
    dynamicGeom.resize(nxt+1);
    PerDynamicGeomInfo& info = dynamicGeom.back();

    info.actor = vtkActor::New();
    info.line  = g;
    info.body1 = b1; info.body2 = b2;
    info.station1 = station1; info.station2 = station2;

    // Now apply the actor-level properties from the geometry.
    const Vec3 color = (info.line.getColor()[0] != -1 ? info.line.getColor() : Black); 
    info.actor->GetProperty()->SetColor(color[0],color[1],color[2]);

    const Real opacity = (info.line.getOpacity() != -1 ? info.line.getOpacity() : Real(1));
    info.actor->GetProperty()->SetOpacity(opacity);

    const Real lineWidth = (info.line.getLineThickness() != -1 ? info.line.getLineThickness() : Real(1));
    info.actor->GetProperty()->SetLineWidth(lineWidth);

    const int representation = (info.line.getRepresentation() != -1 ? info.line.getRepresentation() : VTK_SURFACE);
    info.actor->GetProperty()->SetRepresentation(representation);

    // Set up the mapper & register actor with renderer, but don't set up mapper's input yet.
    vtkPolyDataMapper* mapper = vtkPolyDataMapper::New();
    info.actor->SetMapper(mapper);
    mapper->Delete(); mapper=0; // remove now-unneeded mapper reference
    renderer->AddActor(info.actor);
}

VTKReporterRep::VTKReporterRep(const MultibodySystem& m, bool generateDefaultGeometry) 
    : myHandle(0), defaultGeometryEnabled(generateDefaultGeometry), mbs(m) 
{
    zeroPointers();

    renWin = vtkRenderWindow::New();
    renWin->SetSize(1200,900);
    
    // an interactor
    vtkRenderWindowInteractor *iren = vtkRenderWindowInteractor::New();
    iren->SetRenderWindow(renWin); 
    vtkInteractorStyleTrackballCamera* style=vtkInteractorStyleTrackballCamera::New(); 
    iren->SetInteractorStyle(style);
    style->Delete();
    iren->Initialize(); // register interactor to pick up windows messages

    renderer = vtkRenderer::New();
    renderer->SetBackground(1,1,1); // white

    vtkLight* light = vtkLight::New();
    light->SetPosition(-1,0,0);
    light->SetFocalPoint(0,0,0);
    light->SetColor(1,1,1);
    light->SetIntensity(.75);
    renderer->AddLight(light);
    light->Delete();

    light = vtkLight::New();
    light->SetPosition(1,0,0);
    light->SetFocalPoint(0,0,0);
    light->SetColor(1,1,1);
    light->SetIntensity(.75);
    renderer->AddLight(light);
    light->Delete();

    light = vtkLight::New();
    light->SetPosition(0,1,1);
    light->SetFocalPoint(0,0,0);
    light->SetColor(1,1,1);
    light->SetIntensity(.75);
    renderer->AddLight(light);
    light->Delete();

    renWin->AddRenderer(renderer);

    const MatterSubsystem& sbs = mbs.getMatterSubsystem();
    bodies.resize(sbs.getNBodies());

    setDefaultBodyColor(GroundBodyNum, DefaultGroundBodyColor);
    for (int i=1; i<(int)bodies.size(); ++i) {
        const int parent = sbs.getParent(i);

        if (parent == GroundBodyNum)
             setDefaultBodyColor(i, DefaultBaseBodyColor);
        else setDefaultBodyColor(i, DefaultBodyColor);

        const Transform& jInb = sbs.getJointFrame(State(), i);
        if (jInb.T().norm() > bodies[i].scale)
            bodies[i].scale = jInb.T().norm();
        const Transform& jParent = sbs.getJointFrameOnParent(State(), i);
        if (jParent.T().norm() > bodies[parent].scale)
            bodies[parent].scale = jParent.T().norm();
    }

    if (!defaultGeometryEnabled) {
        renWin->Render();
        return;
    }


    for (int i=0; i<(int)bodies.size(); ++i) {
        const Real scale = bodies[i].scale;
        DecorativeFrame axes(scale*0.5);
        axes.setLineThickness(2);
        addDecoration(i, Transform(), axes); // the body frame

        // Display the inboard joint frame (at half size), unless it is the
        // same as the body frame. Then find the corresponding frame on the
        // parent and display that in this body's color.
        if (i > 0) {
            const int parent = sbs.getParent(i);
            const Real pscale = bodies[parent].scale;
            const Transform& jInb = sbs.getJointFrame(State(), i);
            if (jInb.T() != Vec3(0) || jInb.R() != Mat33(1)) {
                addDecoration(i, jInb, DecorativeFrame(scale*0.25));
                if (jInb.T() != Vec3(0))
                    addDecoration(i, Transform(), DecorativeLine(Vec3(0), jInb.T()));
            }
            const Transform& jParent = sbs.getJointFrameOnParent(State(), i);
            DecorativeFrame frameOnParent(pscale*0.25);
            frameOnParent.setColor(getDefaultBodyColor(i));
            addDecoration(sbs.getParent(i), jParent, frameOnParent);
            if (jParent.T() != Vec3(0))
                addDecoration(sbs.getParent(i), Transform(), DecorativeLine(Vec3(0),jParent.T()));
        }

        // Put a little purple wireframe sphere at the COM, and add a line from 
        // body origin to the com.

        DecorativeSphere com(scale*.05);
        com.setColor(Purple);
        com.setRepresentationToPoints();
        const Vec3& comPos_B = sbs.getBodyCenterOfMassStation(State(), i);
        addDecoration(i, Transform(comPos_B), com);
        if (comPos_B != Vec3(0))
            addDecoration(i, Transform(), DecorativeLine(Vec3(0), comPos_B));
    }
    renWin->Render();
}

void VTKReporterRep::setCameraDefault() {
    renderer->ResetCamera();
    Vec3 pos;
    renderer->GetActiveCamera()->GetPosition(pos[0],pos[1],pos[2]);
    pos *= 1.5;
    renderer->GetActiveCamera()->SetPosition(/*pos[0],pos[1],*/0,0.1*pos[2],pos[2]);
    Real nearClip, farClip;
    renderer->GetActiveCamera()->GetClippingRange(nearClip,farClip);
    renderer->GetActiveCamera()->SetClippingRange(nearClip/10, farClip*10);

}

void VTKReporterRep::report(const State& s) {
    if (!renWin) return;

    mbs.realize(s, Stage::Configured); // just in case

    const MatterSubsystem& matter = mbs.getMatterSubsystem();
    for (int i=1; i<matter.getNBodies(); ++i) {
        const Transform& config = matter.getBodyConfiguration(s, i);
        setConfiguration(i, config);
    }
    for (int i=0; i<(int)dynamicGeom.size(); ++i) {
        const PerDynamicGeomInfo& info = dynamicGeom[i];
        const Transform& X_GB1 = 
            matter.getBodyConfiguration(s, info.body1);
        const Transform& X_GB2 = 
            matter.getBodyConfiguration(s, info.body2);
        setRubberBandLine(i, X_GB1*info.station1, X_GB2*info.station2);
    }

    renWin->Render();

    // Process any window messages since last time
    //TODO: Win32 specific
    MSG msg;
    bool done = false;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            printf("quit!!\n");
            renWin->Delete();renWin=0;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

}

void VTKReporterRep::zeroPointers() {
    renWin=0; renderer=0;
}
void VTKReporterRep::deletePointers() {
    // Delete all the actors. The geometry gets deleted automatically
    // thanks to good design!
    for (int i=0; i<(int)bodies.size(); ++i) {
        std::vector<vtkProp3D*>& actors = bodies[i].aList;
        for (int a=0; a<(int)actors.size(); ++a)
            actors[a]->Delete(), actors[a]=0;
    }
    for (int i=0; i<(int)dynamicGeom.size(); ++i) {
        dynamicGeom[i].actor->Delete();
        dynamicGeom[i].actor = 0;
    }

    if(renderer)renderer->Delete();
    if(renWin)renWin->Delete();
    zeroPointers();
}

void VTKReporterRep::setConfiguration(int bodyNum, const Transform& X_GB) {
    const std::vector<vtkProp3D*>& actors = bodies[bodyNum].aList;
    for (int i=0; i < (int)actors.size(); ++i) {
        vtkProp3D*       actor = actors[i];
        actor->SetPosition(X_GB.T()[0], X_GB.T()[1], X_GB.T()[2]);
        const Vec4 av = X_GB.R().convertToAngleAxis();
        actor->SetOrientation(0,0,0);
        actor->RotateWXYZ(av[0]/RadiansPerDegree, av[1], av[2], av[3]);
    }
}

// Provide two points in ground frame and generate the appropriate line between them.
void VTKReporterRep::setRubberBandLine(int dgeom, const Vec3& p1, const Vec3& p2) {
    vtkActor*       actor = dynamicGeom[dgeom].actor;
    DecorativeLine& line  = dynamicGeom[dgeom].line;
    line.setEndpoints(p1, p2);
    vtkPolyDataMapper::SafeDownCast(actor->GetMapper())->SetInput(line.updVTKPolyData());
}

} // namespace SimTK

