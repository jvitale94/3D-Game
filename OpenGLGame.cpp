#define _USE_MATH_DEFINES
#include <math.h>
#include <stdlib.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
// Needed on MsWindows
#define NOMINMAX
#include <windows.h>
#endif // Win32 platform

#include <math.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>

#include "float2.h"
#include "float3.h"
#include "float4.h"
#include "float4x4.h"
#include "Mesh.h"
#include "Mesh.cpp"
#include "stb_image.c"
#include <vector>

extern "C" unsigned char* stbi_load(char const *filename, int *x, int *y, int *comp, int req_comp);

class LightSource
{
public:
    virtual float3 getRadianceAt  ( float3 x )=0;
    virtual float3 getLightDirAt  ( float3 x )=0;
    virtual float  getDistanceFrom( float3 x )=0;
    virtual void   apply( GLenum openglLightName )=0;
};

class DirectionalLight : public LightSource
{
    float3 dir;
    float3 radiance;
public:
    DirectionalLight(float3 dir, float3 radiance)
    :dir(dir), radiance(radiance){}
    float3 getRadianceAt  ( float3 x ){return radiance;}
    float3 getLightDirAt  ( float3 x ){return dir;}
    float  getDistanceFrom( float3 x ){return 900000000;}
    void   apply( GLenum openglLightName )
    {
        float aglPos[] = {dir.x, dir.y, dir.z, 0.0f};
        glLightfv(openglLightName, GL_POSITION, aglPos);
        float aglZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glLightfv(openglLightName, GL_AMBIENT, aglZero);
        float aglIntensity[] = {radiance.x, radiance.y, radiance.z, 1.0f};
        glLightfv(openglLightName, GL_DIFFUSE, aglIntensity);
        glLightfv(openglLightName, GL_SPECULAR, aglIntensity);
        glLightf(openglLightName, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(openglLightName, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_QUADRATIC_ATTENUATION, 0.0f);
    }
};

class PointLight : public LightSource
{
    float3 pos;
    float3 power;
public:
    PointLight(float3 pos, float3 power)
    :pos(pos), power(power){}
    float3 getRadianceAt  ( float3 x ){return power*(1/(x-pos).norm2()*4*3.14);}
    float3 getLightDirAt  ( float3 x ){return (pos-x).normalize();}
    float  getDistanceFrom( float3 x ){return (pos-x).norm();}
    void   apply( GLenum openglLightName )
    {
        float aglPos[] = {pos.x, pos.y, pos.z, 1.0f};
        glLightfv(openglLightName, GL_POSITION, aglPos);
        float aglZero[] = {0.0f, 0.0f, 0.0f, 0.0f};
        glLightfv(openglLightName, GL_AMBIENT, aglZero);
        float aglIntensity[] = {power.x, power.y, power.z, 1.0f};
        glLightfv(openglLightName, GL_DIFFUSE, aglIntensity);
        glLightfv(openglLightName, GL_SPECULAR, aglIntensity);
        glLightf(openglLightName, GL_CONSTANT_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_LINEAR_ATTENUATION, 0.0f);
        glLightf(openglLightName, GL_QUADRATIC_ATTENUATION, 0.25f / 3.14f);
    }
};

class Material
{
public:
    float3 kd;			// diffuse reflection coefficient
    float3 ks;			// specular reflection coefficient
    float shininess;	// specular exponent
    Material()
    {
        kd = float3(0.5, 0.5, 0.5) + float3::random() * 0.5;
        ks = float3(1, 1, 1);
        shininess = 15;
    }
    virtual void apply()
    {
        float aglDiffuse[] = {kd.x, kd.y, kd.z, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, aglDiffuse);
        float aglSpecular[] = {kd.x, kd.y, kd.z, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, aglSpecular);
        if(shininess <= 128)
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
        else
            glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 128.0f);
    }
    
};

class TexturedMaterial : public Material
{
protected:
    GLuint textureName;
public:
    TexturedMaterial(const char* filename,
                     GLint filtering = GL_LINEAR_MIPMAP_LINEAR
                     ){
        unsigned char* data;
        int width;
        int height;
        int nComponents = 4;
        
        data = stbi_load(filename, &width, &height, &nComponents, 0);
        
        if(data == NULL) return;
        
        glGenTextures(1, &textureName);  // id generation
        glBindTexture(GL_TEXTURE_2D, textureName);      // binding
        
        if(nComponents == 4)
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data); //uploading
        
        else if(nComponents == 3)
            
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data); //uploading

        
        delete data;
    }
    
    void apply()
    {
        Material::apply();
        
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_TEXTURE_2D);
        
        glBindTexture(GL_TEXTURE_2D, textureName);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,
                        GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexEnvi(GL_TEXTURE_ENV,
                  GL_TEXTURE_ENV_MODE, GL_REPLACE);
    }
};

class Camera
{
public:
    float3 eye;
    
    float3 ahead;
    float3 lookAt;
    float3 right;
    float3 up;
    
    float fov;
    float aspect;
    
    float2 lastMousePos;
    float2 mouseDelta;
    float3 getEye()
    {
        return eye;
    }
    
    Camera()
    {
        eye = float3(6.139252, 80.544754, -139.380997);
        lookAt = float3(6.086628, 80.065331, -138.504990);
        right = float3(1, 0, 0);
        up = float3(0, 1, 0);
        
        fov = 1.1;
        aspect  = 1;
    }
    
    void reset()
    {
        eye = float3(0, 4, -30);
        lookAt = float3(0, 0, 0);
        right = float3(1, 0, 0);
        up = float3(0, 1, 0);
        
        fov = 1.1;
        aspect  = 1;
        
        lastMousePos = float2(0,0);
        mouseDelta = float2(0,0);
    }
    
    void apply()
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov /3.14*180, aspect, 0.1, 500);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(eye.x, eye.y, eye.z, lookAt.x, lookAt.y, lookAt.z, 0.0, 1.0, 0.0);
    }
    
    void altApply(float a, float b, float c, float d, float e, float f)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(fov /3.14*180, aspect, 0.1, 500);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        gluLookAt(a,b,c,d,e,f,0.0, 1.0, 0.0);
    }
    
    void setEye(float3 f)
    {
        eye = f;
    }
    
    void setLookAt(float3 f)
    {
        lookAt = f;
    }
    
    void setAspectRatio(float ar) { aspect= ar; }
    
    void move(float dt, std::vector<bool>& keysPressed)
    {
        if(keysPressed.at('w'))
            eye += ahead * dt * 20;
        if(keysPressed.at('s'))
            eye -= ahead * dt * 20;
        if(keysPressed.at('a'))
            eye -= right * dt * 20;
        if(keysPressed.at('d'))
            eye += right * dt * 20;
        if(keysPressed.at('q'))
            eye -= float3(0,1,0) * dt * 20;
        if(keysPressed.at('e'))
            eye += float3(0,1,0) * dt * 20;
        
        float yaw = atan2f( ahead.x, ahead.z );
        float pitch = -atan2f( ahead.y, sqrtf(ahead.x * ahead.x + ahead.z * ahead.z) );
        
        yaw -= mouseDelta.x * 0.02f;
        pitch += mouseDelta.y * 0.02f;
        if(pitch > 3.14/2) pitch = 3.14/2;
        if(pitch < -3.14/2) pitch = -3.14/2;
        
        mouseDelta = float2(0, 0);
        
        ahead = float3(sin(yaw)*cos(pitch), -sin(pitch), cos(yaw)*cos(pitch) );
        right = ahead.cross(float3(0, 1, 0)).normalize();
        up = right.cross(ahead);
        
        lookAt = eye + ahead;
    }
    
    void startDrag(int x, int y)
    {
        lastMousePos = float2(x, y);
    }
    void drag(int x, int y)
    {
        float2 mousePos(x, y);
        mouseDelta = mousePos - lastMousePos;
        lastMousePos = mousePos;
    }
    void endDrag()
    {
        mouseDelta = float2(0, 0);
    }
    void printCamera()
    {
        printf("eye is: (%f  %f  %f)\n",eye.x, eye.y, eye.z);
        printf("lookAt is: (%f  %f  %f)\n",lookAt.x, lookAt.y, lookAt.z);
    }
    
    
    
};

class Object
{
protected:
    Material* material;
    float3 scaleFactor;
    float3 position;
    float3 orientationAxis;
    float orientationAngle;
public:
    Object(Material* material):material(material),orientationAngle(0.0f),scaleFactor(1.0,1.0,1.0),orientationAxis(0.0,1.0,0.0){}
    virtual ~Object(){}
    Object* translate(float3 offset){
        position += offset; return this;
    }
    Object* scale(float3 factor){
        scaleFactor *= factor; return this;
    }
    Object* rotate(float angle){
        orientationAngle += angle; return this;
    }
    virtual void draw()
    {
        material->apply();
        // apply scaling, translation and orientation
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        drawModel();
        glPopMatrix();
    }
    
    virtual void drawModel()=0;
    virtual void move(double t, double dt){}
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects){return false;}
    virtual float3 getCenter()=0;
    virtual float getRadius() = 0;
    virtual bool isCollision(Object* object)
    {
        return false;
    }
    
    virtual void drawShadow(float3 lightDir)
    {
        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();

        glTranslatef(0, 0.01, 0);
        glScalef(1, 0.01, 1);
        
        glTranslatef(position.x, position.y, position.z);
        glRotatef(orientationAngle, orientationAxis.x, orientationAxis.y, orientationAxis.z);
        glScalef(scaleFactor.x, scaleFactor.y, scaleFactor.z);
        
        float shear[] = {
            1, 0, 0, 0,
            lightDir.x/lightDir.y, 1, lightDir.z/lightDir.y, 0,
            0, 0, 1, 0,
            0, 0, 0, 1 };
        glMultMatrixf(shear);
        
        
        drawModel();
        glPopMatrix();
    }
    float3 getPosition()
    {
        return position;
    }
    
    void changeMaterial(Material* mat)
    {
        material = mat;
    }
};

class Ground : public Object
{
public:
    Ground (Material* m):Object(m){}


    void drawModel()
    {
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        
        glBegin(GL_QUADS);
        
        float3 color (rand()/rand(),rand()/rand(),rand()/rand());
        
        glColor3d(0.1, 0.7, 0.3);
        
        glVertex3d(-10000,0,-10000);
        glVertex3d(10000,0,-10000);
        glVertex3d(10000,0,10000);
        glVertex3d(-10000,0,10000);
        
        glEnd();
        
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);

    }
    
    virtual void drawShadow(float3 lightDir)
    {
    }
    
    float3 getCenter()
    {
        return float3(0,0,0);
    }
    float getRadius()
    {
        return 0;
    }
};
    
    


class MeshInstance : public Object
{
protected:
    Mesh* mesh;
public:
    MeshInstance(Material* material, Mesh* m):Object(material), mesh(m){}
    void drawModel()
    {
        mesh->draw();
        
    }
    
    //Next 4 methods not used in this 3D game
    float3 getCenter()
    {
        float x = 0;
        float y = 0;
        float z = 0;
        float n = mesh->positions.size();
        for (int i = 0; i<n; i++)
        {
            float3* point = mesh->at(i);
            x +=point->x;
            y +=point->y;
            z +=point->z;
        }
        
        return float3(x/n, y/n, z/n);
    }
    
    
    
    float distance(float3 other)
    {
        float3 center = getCenter();
        
        float xtemp = center.x-other.x;
        float ytemp = center.y-other.y;
        float ztemp = center.z-other.z;
        
        return sqrtf(xtemp*xtemp+ytemp*ytemp+ztemp*ztemp);
        
    }
    
    float getRadius()
    {
        float radius = 0;
        float n = mesh->positions.size();
        for (int i = 0; i<n; i++)
        {
            float3 point = *mesh->positions.at(i);
            float temprad = distance(point);
            if (temprad>radius)
                radius = temprad;
        }
        return radius;
    }
    
    bool isCollision(Object* other)
    {
        float centers = distance(other->getCenter());
        float radii = getRadius()+other->getRadius();
        return (centers<radii);
    }
};


//Game physics are mainly in this class
class Bouncer : public MeshInstance
{
protected:
    float3 velocity;
    float3 acceleration;
    float angularVelocity;
    float restitution;
    float angularAcceleration;
    int teapots;
public:
    Bouncer(Material* material, Mesh* m):MeshInstance(material, m)
    {
        velocity = float3 (0,0,0);
        angularVelocity = 0;
        restitution = 0.95;
    }
    
    void reset()
    {
        velocity = float3 (0,0,0);
        angularVelocity = 0;
        angularAcceleration = 0;
        acceleration = float3(0,0,0);
        position = float3(0,0,0);
        orientationAngle = 0;
    }
    
    void move(double t, double dt)
    {
        velocity = velocity + acceleration * dt;
        if(position.y < 0)
            velocity.y *= -restitution;
        //velocity.y = 0;
        position = position + velocity * dt;
        
        angularVelocity += angularAcceleration * dt;
        angularVelocity *= pow (0.5,dt);
        velocity *= pow (0.2,dt);
        
        orientationAngle += angularVelocity * dt;
        if (size(position)>1000)
        {
            reset();
        }
    }
    
    
    
    float size(float3 f)
    {
        return sqrtf(f.x*f.x+f.y*f.y+f.z*f.z);
    }
    
    float3 getVelocity()
    {
        return velocity;
    }
    
    void setVelocity(float3 v)
    {
        velocity = v;
    }
    
    float3 getAcceleration()
    {
        return acceleration;
    }
    
    void setAcceleration(float3 v)
    {
        acceleration = v;
    }

    
    float getOrientationAngle()
    {
        return orientationAngle;
    }
    
    virtual bool control(std::vector<bool>& keysPressed, std::vector<Object*>& spawn, std::vector<Object*>& objects)
    {
        if (keysPressed.at('h'))
        {
            angularAcceleration += 10;
        }
        else if (keysPressed.at('k'))
        {
            angularAcceleration -= 10;
        }
        else
        {
            angularAcceleration = 0;
        }
        
        
        if (keysPressed.at(32) && keysPressed.at('u'))
        {
            acceleration = float3(-cos(orientationAngle*3.14/180)*10, 0, sin(orientationAngle*3.14/180) *10)*8;
        }
        else if (keysPressed.at(32) && keysPressed.at('j'))
        {
            acceleration = float3(cos(orientationAngle*3.14/180) *10, 0, -sin(orientationAngle*3.14/180) *10)*8;
        }
        else if (keysPressed.at('u'))
        {
            acceleration = float3(-cos(orientationAngle*3.14/180)*10, 0, sin(orientationAngle*3.14/180) *10);
        }

        else if (keysPressed.at('j'))
        {
            acceleration = float3(cos(orientationAngle*3.14/180) *10, 0, -sin(orientationAngle*3.14/180) *10);
        }
        else
        {
            acceleration = float3(0,0,0);
        }
        
        
        if (keysPressed.at('f'))
        {
            angularVelocity = 0;
            angularAcceleration = 0;
        }
        if (keysPressed.at('r'))
        {
            reset();
        }
        
        return false;
    }
    
    
};


class Teapot : public Object
{
public:
    Teapot(Material* material):Object(material){}
    void drawModel()
    {
        glutSolidTeapot(1.0f);
    }
    
    float3 getCenter()
    {
        return float3(0,0,0);
    }
    float getRadius()
    {
        return 0;
    }
};

class Scene
{
    Camera camera;
    std::vector<LightSource*> lightSources;
    std::vector<Object*> objects;
    std::vector<Material*> materials;
    Bouncer* avatar;
    float3 avatarPos;
    std::vector<float3> treePositions;
    std::vector<float3> orbPositions;
    std::vector<int> hitIndices;
    float scaleFactor = 0.5;
    int hitOrbs = 0;
public:
    void initialize()
    {
        // BUILD YOUR SCENE HERE
        lightSources.push_back(
                               new DirectionalLight(
                                                    float3(5, 6, 5),
                                                    float3(1, 1, 1)));
//        lightSources.push_back(
//                               new PointLight(
//                                              float3(-1, -1, 1),
//                                              float3(0.2, 0.1, 0.1)));
        Material* yellowDiffuseMaterial = new Material();
        materials.push_back(yellowDiffuseMaterial);
        yellowDiffuseMaterial->kd = float3(1, 1, 0);
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        materials.push_back(new Material());
        
        Material* tiggerMaterial = new TexturedMaterial("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/tigger.png");
        Mesh* tiggerMesh = new Mesh("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/tigger.obj");
        
        
        Material* treeMaterial = new TexturedMaterial("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/tree.png");
        Mesh* treeMesh = new Mesh("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/tree.obj");
        
        Material* orbMaterial = new TexturedMaterial("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/bullet.png");
        
        Material* selectedOrbMaterial = new TexturedMaterial("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/bullet2.png");
        
        Material* groundMaterial = new TexturedMaterial("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/asteroid2.png");
        
         materials.push_back(tiggerMaterial);
        materials.push_back(treeMaterial);
        
        
        avatar = new Bouncer(tiggerMaterial, tiggerMesh);
        
        avatarPos = avatar->getPosition();
        
        objects.push_back(((avatar)->scale(float3(0.5,0.5,0.5)))->translate(float3 (0,0,0)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(-50, 0, -50)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(-50, 0, 50)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(50, 0, -50)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(50, 0, 50)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(-100, 0, -100)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(-100, 0, 100)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(100, 0, -100)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(100, 0, 100)));
        
        objects.push_back(((new MeshInstance(treeMaterial, treeMesh))->scale(float3(0.5,0.5,0.5)))->translate(float3(-75, 0, 30)));


        
        objects.push_back( (new Teapot(selectedOrbMaterial))->translate(float3(40, 2, 0.5))->scale(float3(3, 4, 2)) );
        
        objects.push_back( (new Teapot(orbMaterial))->translate(float3(-40, 2, 0.5))->scale(float3(3, 4, 2)) );
        
        objects.push_back( (new Teapot(orbMaterial))->translate(float3(60, 2, 0.5))->scale(float3(3, 4, 2)) );
        
        objects.push_back( (new Teapot(orbMaterial))->translate(float3(0, 2, 40.5))->scale(float3(3, 4, 2)) );
        
        objects.push_back( (new Teapot(orbMaterial))->translate(float3(0, 2, -30.5))->scale(float3(3, 4, 2)) );
        
        objects.push_back( (new Teapot(orbMaterial))->translate(float3(-90, 2, 30.5))->scale(float3(3, 4, 2)) );
        
        objects.push_back( (new Teapot(orbMaterial))->translate(float3(10, 2, 0.5))->scale(float3(3, 4, 2)) );
        
        
        
        
        
        
        for (int i = 1; i<objects.size()-7; i++)
        {
            float3 pos = objects[i]->getPosition();
            treePositions.push_back(pos);
        }
        
        for (int i = objects.size()-7; i<objects.size(); i++)
        {
            float3 pos = objects[i]->getPosition();
            orbPositions.push_back(pos);
        }
        
        std::vector<int> temp(orbPositions.size(),0);
        hitIndices = temp;
        
        
        
        
        
        Ground *ground = new Ground(groundMaterial);
        objects.push_back(ground);
        
        for (int i = 0; i<treePositions.size(); i++)
        {
            float3 pos = treePositions[i];
        }
        
    }
    ~Scene()
    {
        for (std::vector<LightSource*>::iterator iLightSource = lightSources.begin(); iLightSource != lightSources.end(); ++iLightSource)
            delete *iLightSource;
        for (std::vector<Material*>::iterator iMaterial = materials.begin(); iMaterial != materials.end(); ++iMaterial)
            delete *iMaterial;
        for (std::vector<Object*>::iterator iObject = objects.begin(); iObject != objects.end(); ++iObject)
            delete *iObject;
    }
    
public:
    
    
    Camera& getCamera()
    {
        return camera;
    }
    
    void setCameraLookAt()
    {
        float3 f = avatar->getPosition();
        camera.setLookAt(float3(f.x, 3 ,f.z));
    }
    
    void setCameraEye()
    {
        float3 pos = avatar->getPosition();
        float3 vel = avatar->getVelocity();
        float angle = avatar->getOrientationAngle();
        camera.setEye(float3(pos.x+sin(angle-3.14/180-3.14/2),
                             10,
                             pos.z+cos(angle-3.14/180-3.14/2)));
    }
    
    void draw()
    {
        //position.x+cos(orienationangle *3.14/180)*10
        camera.apply();
        unsigned int iLightSource=0;
        for (; iLightSource<lightSources.size(); iLightSource++)
        {
            glEnable(GL_LIGHT0 + iLightSource);
            lightSources.at(iLightSource)->apply(GL_LIGHT0 + iLightSource);
        }
        for (; iLightSource<GL_MAX_LIGHTS; iLightSource++)
            glDisable(GL_LIGHT0 + iLightSource);
        
        for (unsigned int iObject=0; iObject<objects.size(); iObject++)
            objects.at(iObject)->draw();
        
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        
        glColor3d(0.0, 0.0, 0.0);
        
        float3 lightDir =
        lightSources.at(0)
        ->getLightDirAt(float3(0, 0, 0));
        
        for (unsigned int iObject=0; iObject<objects.size(); iObject++)
            objects.at(iObject)->drawShadow(lightDir);
        
        glEnable(GL_LIGHTING);
        glEnable(GL_TEXTURE_2D);
    }
    
    void move(float t, float dt)
    {
        for (unsigned int iObject=0; iObject<objects.size(); iObject++)
            objects.at(iObject)->move(t,dt);
    }
    
    float distance(float3 first, float3 other)
    {
        float xtemp = first.x-other.x;
        float ytemp = first.y-other.y;
        float ztemp = first.z-other.z;
        
        return sqrtf(xtemp*xtemp+ytemp*ytemp+ztemp*ztemp);
        
    }
    
    void checkCollisions()
    {
        avatarPos = avatar->getPosition();
        
        int hitIndex = 0;
        
        for (int i = 0; i<treePositions.size(); i++)
        {
            float dist = distance(avatarPos, treePositions[i]);
            if (dist<14)
            {
                avatar->setAcceleration(avatar->getAcceleration()*1);
                avatar->setVelocity(avatar->getVelocity()*-2);
                
            }
        }
        
        for (int i = 0; i<orbPositions.size(); i++)
        {
            float dist = distance(avatarPos, orbPositions[i]);
            if (dist<6 && hitIndices[i]==0 && hitOrbs == i)
            {
                hitOrbs++;
                hitIndices[i] = 1;
                hitIndex = i;
                objects.at(treePositions.size()+i+1)->translate(float3 (0,-200,0));
                avatar->scale(float3(1/scaleFactor,1/scaleFactor,1/scaleFactor));
                scaleFactor+=0.1;
                avatar->scale(float3(scaleFactor,scaleFactor,scaleFactor));
                objects[treePositions.size()+hitOrbs+1]->changeMaterial(new TexturedMaterial("/Users/jakevitale/Documents/Comp Sci/Computer Graphics/OpenGL/OpenGL/bullet2.png"));
                
            }
        }
        
        if (hitOrbs == orbPositions.size())
        {
            avatar->setVelocity(float3(0,30,0));
        }
    }
    
    void control(std::vector<bool>& keysPressed)
    {
        std::vector<Object*> spawn;
        for (unsigned int iObject=0;
             iObject<objects.size(); iObject++)
            objects.at(iObject)->control(keysPressed, spawn, objects);
    }
};

Scene scene;
std::vector<bool> keysPressed;

void onDisplay( ) {
    glClearColor(0.1f, 0.3f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear screen
    
    scene.draw();
    
    if (keysPressed.at('p'))
    {
        scene.getCamera().printCamera();
    }
    
    if (keysPressed.at('r'))
    {
        //scene.getCamera().reset();
    }
    
    glutSwapBuffers(); // drawing finished
}

void onIdle()
{
    double t = glutGet(GLUT_ELAPSED_TIME) * 0.001;        	// time elapsed since starting this program in msec
    static double lastTime = 0.0;
    double dt = t - lastTime;
    lastTime = t;
    
    scene.getCamera().move(dt, keysPressed);
    
    scene.control(keysPressed);
    scene.move(t,dt);
//    scene.setCameraEye();
//    scene.setCameraLookAt();
    scene.checkCollisions();
    
    glutPostRedisplay();
}

void onKeyboard(unsigned char key, int x, int y)
{
    keysPressed.at(key) = true;
}

void onKeyboardUp(unsigned char key, int x, int y)
{
    keysPressed.at(key) = false;
}

void onMouse(int button, int state, int x, int y)
{
    if(button == GLUT_LEFT_BUTTON)
        if(state == GLUT_DOWN)
            scene.getCamera().startDrag(x, y);
        else
            scene.getCamera().endDrag();
}

void onMouseMotion(int x, int y)
{
    scene.getCamera().drag(x, y);
}

void onReshape(int winWidth, int winHeight)
{
    glViewport(0, 0, winWidth, winHeight);
    scene.getCamera().setAspectRatio((float)winWidth/winHeight);
}	

int main(int argc, char **argv) {
    glutInit(&argc, argv);						// initialize GLUT
    glutInitWindowSize(600, 600);				// startup window size 
    glutInitWindowPosition(100, 100);           // where to put window on screen
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);    // 8 bit R,G,B,A + double buffer + depth buffer
    
    glutCreateWindow("OpenGL Game");				// application window is created and displayed
    
    glViewport(0, 0, 600, 600);
    
    glutDisplayFunc(onDisplay);					// register callback
    glutIdleFunc(onIdle);						// register callback
    glutReshapeFunc(onReshape);
    glutKeyboardFunc(onKeyboard);
    glutKeyboardUpFunc(onKeyboardUp);
    glutMouseFunc(onMouse);
    glutMotionFunc(onMouseMotion);
    
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_NORMALIZE);
    
    scene.initialize();
    for(int i=0; i<256; i++)
        keysPressed.push_back(false);
    
    glutMainLoop();								// launch event handling loop
    
    return 0;
}
