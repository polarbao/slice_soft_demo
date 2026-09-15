#include "camera/CameraController.h"
#include "render/CpuRasterBackend.h"
#include "render/TopViewRenderPolicy.h"
#include "ThreeDCanvasWidget.h"
#include "HostResponsiveForms.h"
#include "HostRipSettingsPanel.h"
#include "render/SelectionOutline.h"
#include <QApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QLineEdit>
#include <QFileInfo>
#include <QTextStream>
#include <array>
#include <cmath>
#include <stdexcept>

void RunHostWheelPolicyTests();

namespace
{
void Require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
void Identity(float* matrix) { for(int i=0;i<16;++i) matrix[i]=i%5==0?1.0F:0.0F; }
QPointF Project(const slicer::render::CameraDesc& c,const std::array<float,3>& p)
{
    float view[4]{},clip[4]{}; const float point[4]{p[0],p[1],p[2],1};
    for(int r=0;r<4;++r) for(int k=0;k<4;++k) view[r]+=c.viewMatrix[r*4+k]*point[k];
    for(int r=0;r<4;++r) for(int k=0;k<4;++k) clip[r]+=c.projMatrix[r*4+k]*view[k];
    return {clip[0]/clip[3],clip[1]/clip[3]};
}
void CameraTest()
{
    for(auto projection:{slicer::render::Projection::Orthographic,slicer::render::Projection::Perspective})
    {
        CameraController camera; camera.Fit({0,0,0,100,80,20},800,480); camera.SetProjection(projection);
        const std::array<float,3> point{33,21,7};
        const auto before=Project(camera.BuildCamera(),point);
        camera.OrbitAround(-15,-10,point);
        const auto after=Project(camera.BuildCamera(),point);
        Require((before-after).manhattanLength()<1e-5,"orbit anchor moved on screen");
        const auto plane=camera.PointOnTargetPlane(0.35F,-0.2F);
        Require((Project(camera.BuildCamera(),plane)-QPointF(0.35,-0.2)).manhattanLength()<1e-5,"focus plane mismatch");
    }
}
void RasterTest(const QString& output)
{
    using namespace slicer::render;
    CpuRasterBackend backend;
    const float positions[]{-0.6F,-0.6F,0, 0.6F,-0.6F,0, 0,0.6F,0};
    const float normals[]{0,0,1,0,0,1,0,0,1}; const float uv[]{0,0,1,0,0.5F,1};
    const std::uint32_t indices[]{0,1,2};
    MeshDesc mesh; mesh.meshIdentity="mesh"; mesh.vertexCount=3; mesh.triangleCount=1;
    mesh.position=positions; mesh.normal=normals; mesh.texcoord0=uv; mesh.index=indices;
    mesh.submeshes.push_back({0,3,"mat"}); Require(backend.UploadMesh(mesh),"mesh upload");
    MaterialDesc material; material.appearanceIdentity="look"; material.materialId="mat";
    Require(backend.UploadMaterial(material),"material upload");
    FrameDesc frame; frame.viewportWidthPx=400; frame.viewportHeightPx=300;
    frame.decor.showGrid=false; frame.decor.showAxes=false; frame.decor.showBuildVolume=false;
    Identity(frame.camera.viewMatrix); Identity(frame.camera.projMatrix);
    InstanceDraw back; back.instanceId="back"; back.meshIdentity="mesh"; back.appearanceIdentity="look";
    Identity(back.worldMatrix); back.worldMatrix[11]=0.5F;
    auto front=back; front.instanceId="front"; front.worldMatrix[11]=-0.25F;
    frame.instances={front,back};
    const auto picked=backend.Pick(frame,200,150);
    Require(picked.hit && picked.instanceId=="front" && std::abs(picked.worldPosMm[2]+0.25F)<1e-5,"nearest surface picking");
    Require(!backend.Pick(frame,310,70).hit,"bounding rectangle false positive");
    ImageOut normal,selected; Require(backend.RenderToImage(frame,normal),"normal render");
    frame.instances[0].selected=true;
    Require(backend.RenderToImage(frame,selected),"selected render");
    int edges=0;
    for(std::size_t i=0;i<selected.rgba8.size();i+=4)
        if(selected.rgba8[i]==32 && selected.rgba8[i+1]==144 && selected.rgba8[i+2]==255) ++edges;
    Require(edges>100,"missing selected contour");
    const auto center=(150*400+200)*4;
    Require(selected.rgba8[center]==normal.rgba8[center],"selection changed interior material");
    QImage image(selected.rgba8.data(),400,300,QImage::Format_RGBA8888);
    Require(image.save(output+"/selection3d.png"),"save raster evidence");
    ThreeDCanvasWidget canvas; canvas.resize(800,480); canvas.SetSceneBounds({0,0,0,100,80,20});
    canvas.SetImage(image.copy()); canvas.show(); QApplication::processEvents();
    Require(canvas.grab().save(output+"/axes3d.png"),"save axes evidence");
}
void TopTest(const QString& output)
{
    ModuleClient client; TopViewRenderPolicy policy(client);
    TopViewFrame frame; frame.viewDataIdentity="fixture"; frame.sceneRevision=1;
    frame.decor.buildWidthMm=100; frame.decor.buildHeightMm=60;
    TopViewInstance instance; instance.instanceId="one"; instance.previewIdentity="p";
    instance.localBoundsMm={0,0,20,20};
    for(int i=0;i<16;++i) instance.worldMatrix[i]=i%5==0?1:0;
    instance.worldMatrix[3]=20; instance.worldMatrix[7]=20;
    instance.surfacePreview=QImage(80,80,QImage::Format_RGBA8888); instance.surfacePreview.fill(Qt::transparent);
    {QPainter p(&instance.surfacePreview);p.setBrush(Qt::white);p.setPen(Qt::NoPen);p.drawEllipse(5,5,70,70);}
    frame.instances.append(instance);
    const auto plain=policy.Render(frame,{800,480});
    policy.SetSelectedInstances({QStringLiteral("one")});
    const auto chosen=policy.Render(frame,{800,480});
    Require(!chosen.isNull() && plain!=chosen,"top selection not rendered");
    Require(client.CallCount()==0,"selection crossed ABI");
    Require(chosen.save(output+"/selection2d.png"),"save top evidence");
    instance.instanceId=QStringLiteral("occluder");
    frame.instances.append(instance);
    const auto occluded=policy.Render(frame,{800,480});
    policy.SetSelectedInstances({});
    Require(occluded==policy.Render(frame,{800,480}),"hidden top contour leaked through foreground");
    instance.surfacePreview.fill(Qt::transparent);
    {QPainter p(&instance.surfacePreview);p.fillRect(4,4,16,16,Qt::white);}
    instance.worldMatrix[0]=-1;instance.worldMatrix[5]=-1;
    instance.worldMatrix[3]=60;instance.worldMatrix[7]=60;
    frame.instances={instance}; ++frame.localRevision;
    const auto rotated=policy.Render(frame,{800,480});
    int checked=0;
    for(int y=0;y<rotated.height();++y) for(int x=0;x<rotated.width();++x)
        if(rotated.pixelColor(x,y)==QColor(Qt::white))
        {
            Require(policy.PickInstance(frame,{800,480},QPointF(x+0.5,y+0.5))==instance.instanceId,
                "rotated top preview and alpha picking disagree");
            ++checked;
        }
    Require(checked>10,"rotated fixture was blank");
}
void FormTest(const QString& output)
{
    QWidget widget; auto* form=new QFormLayout(&widget); auto* input=new QDoubleSpinBox(&widget);
    input->setRange(-999999,999999);input->setDecimals(3);input->setValue(123456.789);
    form->addRow(QStringLiteral("高分辨率长标签输入字段"),input); ConfigureHostForms(&widget);
    widget.resize(300,150);widget.show();QApplication::processEvents();
    Require(input->height()>=input->sizeHint().height(),"input height squeezed");
    Require(input->width()>=input->minimumWidth(),"input width squeezed");
    Require(input->findChild<QLineEdit*>()->minimumHeight()==0,"internal editor minimum changed");
    const auto* label=qobject_cast<QLabel*>(form->itemAt(0,QFormLayout::LabelRole)->widget());
    Require(label && label->geometry().bottom()<input->geometry().top(),"form label overlaps field");
    Require(label->height()>=label->heightForWidth(label->width()),"wrapped label clipped");
    Require(widget.grab().save(output+"/form.png"),"save form evidence");
    HostRipSettingsPanel panel; panel.ShowProgress(QStringLiteral("运算"),2,5,1200);
    const auto firstOutput=panel.ManualOutputDirectory();
    Require(!firstOutput.isEmpty() && !QFileInfo::exists(firstOutput)
        && QFileInfo(QFileInfo(firstOutput).absolutePath()).isDir(),"manual output default invalid");
    panel.SetPackageDirectory(output+"/package-one");
    Require(QDir::fromNativeSeparators(panel.ManualInputDirectory())==output+"/package-one/layers","latest layers not selected");
    panel.SetPackageDirectory(output+"/package-two");
    Require(QDir::fromNativeSeparators(panel.ManualInputDirectory())==output+"/package-two/layers","latest layers not refreshed");
    auto* manualInput=panel.findChild<QLineEdit*>(QStringLiteral("hostRipManualInputPath"));
    manualInput->setText(output+"/custom-input");
    panel.SetPackageDirectory(output+"/package-three");
    Require(panel.ManualInputDirectory()==output+"/custom-input","user input overwritten");
    panel.ShowCompletion(true,false,QStringLiteral("完成"),firstOutput);
    Require(panel.ManualOutputDirectory()!=firstOutput,"next manual job reuses completed output");
    panel.ShowProgress(QStringLiteral("运算"),2,5,1200);
    auto* bar=panel.findChild<QProgressBar*>(QStringLiteral("hostRipProgressBar"));
    Require(bar && bar->maximum()==5 && bar->value()==2,"RIP file progress");
    panel.ShowProgress(QStringLiteral("输出校验"),-1,5,1400);
    Require(bar->maximum()==0,"validation must not claim completion");
    panel.ShowCompletion(false,true,QStringLiteral("取消"),{});
    Require(bar->maximum()==1 && bar->value()==0,"cancel kept busy progress");
}
void OutlineHoleTest()
{
    std::vector<std::uint8_t> pixels(100*100*4,111),mask(100*100);
    for(int y=10;y<90;++y) for(int x=10;x<90;++x) mask[y*100+x]=1;
    for(int y=35;y<65;++y) for(int x=35;x<65;++x) mask[y*100+x]=0;
    slicer::render::DrawSelectionOutline(pixels.data(),100,100,mask);
    for(int y=25;y<75;++y) for(int x=25;x<75;++x)
        Require(pixels[(y*100+x)*4]==111,"internal transparent artwork was outlined");
    Require(pixels[(10*100+50)*4+2]==255,"outer outline is not blue");
}
}
int main(int argc,char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc,argv);
    const QString output=argc>1?QString::fromLocal8Bit(argv[1]):QStringLiteral("hostux-evidence");
    QDir().mkpath(output);
    try { CameraTest();RasterTest(output);TopTest(output);FormTest(output);OutlineHoleTest();RunHostWheelPolicyTests(); }
    catch(const std::exception& e){QTextStream(stderr)<<e.what()<<Qt::endl;return 1;}
    QTextStream(stdout)<<"HOSTUX_INTERACTION_PASS"<<Qt::endl;return 0;
}
