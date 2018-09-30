#include <QCoreApplication>
#include <QDebug>
#include <QString>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QExposeEvent>
#include "input.h"
#include "window.h"
#include "vertex.h"

#include "glm/glm.hpp"

extern void CUDA_init();
extern void *CUDA_registerBuffer(GLuint buf);
extern void CUDA_unregisterBuffer(void *res);
extern void *CUDA_map(void *res);
extern void CUDA_unmap(void *res);
extern void CUDA_do_something(void *devPtr, int w, int h, float t);

// Create a colored single fullscreen triangle
// 3*______________
//  |\_____________
//  | \____________
// 2*  \___________
//  |   \__________
//  |    \_________
// 1*--*--*________
//  |.....|\_______
//  |.....| \______
// 0*..*..*  \_____
//  |.....|   \____
//  |.....|    \___
//-1*--*--*--*--*__
// -1  0  1  2  3 x position coords
//  0     1     2 s texture coords
static const Vertex sg_vertexes[] = {
    Vertex( QVector3D(-1.0f,  3.0f, -1.0f), QVector3D(0.0f, 2.0f, 0.0f) ),
    Vertex( QVector3D(-1.0f, -1.0f, -1.0f), QVector3D(0.0f, 0.0f, 1.0f) ),
    Vertex( QVector3D( 3.0f, -1.0f, -1.0f), QVector3D(2.0f, 0.0f, 0.0f) )
};

Window::Window()
{
    m_t = 0.0f;
}

Window::~Window()
{
    makeCurrent();
    teardownGL();
}

// ======================================================================
// public
// ======================================================================

void Window::initializeGL()
{
    // Initialize OpenGL Backend
    initializeOpenGLFunctions();
    connect(this, SIGNAL(frameSwapped()), this, SLOT(update()));
    printInfo();

    // Set global information
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // OpenGL initialization
    {
        // Create Shader (Do not release until VAO is created)
        m_program = new QOpenGLShaderProgram();
        m_program->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/shaders/basic_vert.glsl");
        m_program->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/shaders/basic_frag.glsl");
        m_program->link();
        m_program->bind();

        // Create Buffer (Do not release until VAO is created)
        m_vertex.create();
        m_vertex.bind();
        m_vertex.setUsagePattern(QOpenGLBuffer::StaticDraw);
        m_vertex.allocate(sg_vertexes, sizeof(sg_vertexes));

        // Create Vertex Array Object
        m_object.create();
        m_object.bind();
        m_program->enableAttributeArray(0);
        m_program->enableAttributeArray(1);
        m_program->setAttributeBuffer(0, GL_FLOAT, Vertex::positionOffset(), Vertex::PositionTupleSize, Vertex::stride());
        m_program->setAttributeBuffer(1, GL_FLOAT, Vertex::colorOffset(), Vertex::ColorTupleSize, Vertex::stride());

        // Release (unbind) all
        m_object.release();
        m_vertex.release();
        m_program->release();

        // Load texture
        m_texture = new QOpenGLTexture(QImage(QString(":/images/side1.png")).mirrored());

        // Shared OpenGL & CUDA buffer
        // Generate a buffer ID
        glGenBuffers(1,&m_shared_pbo_id);
        // Make this the current UNPACK buffer aka PBO (Pixel Buffer Object)
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_shared_pbo_id);
        // Allocate data for the buffer
        m_shared_width = m_shared_height = 1024;
        glBufferData(GL_PIXEL_UNPACK_BUFFER, m_shared_width * m_shared_height * 4, nullptr, GL_DYNAMIC_COPY);

        // Create a GL Texture
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1,&m_shared_tex_id);
        glBindTexture(GL_TEXTURE_2D, m_shared_tex_id);
        // Allocate the texture memory.
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, m_shared_width, m_shared_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        // Set filter mode
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    }

    // CUDA initialization
    {
        CUDA_init();
        m_cuda_pbo_handle = CUDA_registerBuffer(m_shared_pbo_id);
    }
}

void Window::resizeGL(int width, int height)
{
    /*
   // Set the aspect ratio of the clipping area to match the viewport
   glMatrixMode(GL_PROJECTION);  // To operate on the Projection matrix
   glLoadIdentity();             // Reset the projection matrix
   if (width >= height) {
     // aspect >= 1, set the height from -1 to 1, with larger width
      gluOrtho2D(-1.0 * aspect, 1.0 * aspect, -1.0, 1.0);
   } else {
      // aspect < 1, set the width to -1 to 1, with larger height
     gluOrtho2D(-1.0, 1.0, -1.0 / aspect, 1.0 / aspect);
   }
     */
    // Currently we are not handling width/height changes
    (void)width;
    (void)height;
}

void Window::paintGL()
{
    // Clear
    glClear(GL_COLOR_BUFFER_BIT);

    // Do some CUDA that writes to the pbo
    void *devPtr = CUDA_map(m_cuda_pbo_handle);
    m_t += 1.0f/1.0f;
    CUDA_do_something(devPtr, m_shared_width, m_shared_height, m_t);
    CUDA_unmap(m_cuda_pbo_handle);

    // Render using our shader
    m_program->bind();
    {
        m_object.bind();

        // the equivalent of:
        // m_texture->bind();
        // connect the pbo to the texture
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_shared_pbo_id);
        glBindTexture(GL_TEXTURE_2D, m_shared_tex_id);
        // Since source parameter is NULL, Data is coming from a PBO, not host memory
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_shared_width, m_shared_height, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, sizeof(sg_vertexes) / sizeof(sg_vertexes[0]));
        m_object.release();
    }
    m_program->release();
}

void Window::teardownGL()
{
    // Destroy our CUDA info
    CUDA_unregisterBuffer(m_cuda_pbo_handle);

    // Destroy our OpenGL information
    m_object.destroy();
    m_vertex.destroy();
    delete m_program;
    delete m_texture;
}

// ======================================================================
// protected
// ======================================================================

void Window::update()
{
    // Update input
    Input::update();

    if (Input::keyPressed(Qt::Key_Escape)) {
        QCoreApplication::quit();
    }

    QOpenGLWindow::update();
}

void Window::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
    {
        event->ignore();
    }
    else
    {
        Input::registerKeyPress(event->key());
    }
}

void Window::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat())
    {
        event->ignore();
    }
    else
    {
        Input::registerKeyRelease(event->key());
    }
}

void Window::mousePressEvent(QMouseEvent *event)
{
    Input::registerMousePress(event->button());
}

void Window::mouseReleaseEvent(QMouseEvent *event)
{
    Input::registerMouseRelease(event->button());
}

// ======================================================================
// private
// ======================================================================

void Window::printInfo()
{
    QString glType;
    QString glVersion;
    QString glProfile;

    // Get Version Information
    glType = (context()->isOpenGLES()) ? "OpenGL ES" : "OpenGL";
    glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));

    // Get Profile Information
#define CASE(c) case QSurfaceFormat::c: glProfile = #c; break
    switch (format().profile())
    {
    CASE(NoProfile);
    CASE(CoreProfile);
    CASE(CompatibilityProfile);
    }
#undef CASE

    // qPrintable() will print our QString w/o quotes around it.
    qDebug() << qPrintable(glType) << qPrintable(glVersion) << "(" << qPrintable(glProfile) << ")";
}