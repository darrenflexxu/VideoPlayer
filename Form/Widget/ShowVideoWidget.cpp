#include "showVideoWidget.h"
#include "ui_showVideoWidget.h"
#include <QPainter>
#include <QDebug>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QFile>
#include <QOpenGLTexture>
#include <QOpenGLBuffer>
#include <QMouseEvent>
#include <QTimer>
#include <QDrag>
#include <QMimeData>
#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QDateTime>
#include "AppConfig.h"

#define ATTRIB_VERTEX 3
#define ATTRIB_TEXTURE 4

///用于绘制矩形
//! [3]
static const char *vertexShaderSource =
"attribute highp vec4 posAttr;\n"
"attribute lowp vec4 colAttr;\n"
"varying lowp vec4 col;\n"
"uniform highp mat4 matrix;\n"
"void main() {\n"
"   col = colAttr;\n"
"   gl_Position = posAttr;\n"
"}\n";

static const char *fragmentShaderSource =
"varying lowp vec4 col;\n"
"void main() {\n"
"   gl_FragColor = col;\n"
"}\n";
//! [3]

ShowVideoWidget::ShowVideoWidget(QWidget *parent):
    QOpenGLWidget(parent),
    ui_(new Ui::ShowVideoWidget) {
    ui_->setupUi(this);
    connect(ui_->pushButton_close, &QPushButton::clicked, this, &ShowVideoWidget::sig_CloseBtnClick);
    ui_->pushButton_close->hide();
    is_playing_ = false;
    play_failed_ = false;
    ui_->widget_erro->hide();
    ui_->widget_name->hide();
    texture_uniform_y_ = 0;
    texture_uniform_u_ = 0;
    texture_uniform_v_ = 0;
    id_y_ = 0;
    id_u_ = 0;
    id_v_ = 0;
    vertex_shader_ = NULL;
    fragment_shader_ = NULL;
    shader_program_ = NULL;
    texture_y_ = NULL;
    texture_u_ = NULL;
    texture_v_ = NULL;
    vertex_vertices_ = new GLfloat[8];
    resolution_height_ = 0;
    resolution_width_ = 0;
    pic_index_x_ = 0;
    pic_index_y_ = 0;
    setAcceptDrops(true);
    current_video_keep_aspect_ratio_ = AppConfig::gVideoKeepAspectRatio;
    is_show_face_rect_ = false;
    is_closeable_ = true;
    is_opengl_inited_ = false;
    last_get_frame_time_ = 0;
}

ShowVideoWidget::~ShowVideoWidget() {
    delete ui_;
}

void ShowVideoWidget::setIsPlaying(bool value) {
    is_playing_ = value;
    FunctionTransfer::runInMainThread([=] () {
        if (!is_playing_) {
            ui_->pushButton_close->hide();
        }
        update();
    });
}

void ShowVideoWidget::setPlayFailed(bool value) {
    play_failed_ = value;
    FunctionTransfer::runInMainThread([=] () {
        update();
    });
}

void ShowVideoWidget::setCameraName(QString name) {
    camera_name_ = name;
    FunctionTransfer::runInMainThread([=] () {
        update();
    });
}

void ShowVideoWidget::setVideoWidth(int w, int h) {
    if (w <= 0 || h <= 0) { 
        return; 
    }
    resolution_width_ = w;
    resolution_height_ = h;
    qDebug() << __FUNCTION__ << w << h << this->isHidden();

    if (is_opengl_inited_) {
        FunctionTransfer::runInMainThread([=] () {
            resetGLVertex(this->width(), this->height());
        });
    }
}

void ShowVideoWidget::setCloseAble(bool isCloseAble) {
    is_closeable_ = isCloseAble;
}

void ShowVideoWidget::clear() {
    FunctionTransfer::runInMainThread([=] () {
        ReleaseVideoFrame(video_frame_);
        video_frame_ = nullptr;
        face_info_list_.clear();
        update();
    });
}

void ShowVideoWidget::enterEvent(QEvent *event) {
    if (is_playing_ && is_closeable_) {
        ui_->pushButton_close->show();
    }
}

void ShowVideoWidget::leaveEvent(QEvent *event) {
    ui_->pushButton_close->hide();
}

void ShowVideoWidget::mouseMoveEvent(QMouseEvent *event) {
    if ((event->buttons() & Qt::LeftButton) && is_playing_) {
        QDrag *drag = new QDrag(this);
        QMimeData *mimeData = new QMimeData;
        ///为拖动的鼠标设置一个图片
        QPixmap pixMap = this->grab();
        QString name = camera_name_;

        if (name.isEmpty()) {
            name = "drag";
        }
        QString filePath = QString("%1/%2.png").arg(AppConfig::AppDataPath_Tmp).arg(name);
        pixMap.save(filePath);
        QList<QUrl> list;
        QUrl url = "file:///" + filePath;
        list.append(url);
        mimeData->setUrls(list);
        drag->setPixmap(pixMap);
        ///实现视频画面拖动，激发拖动事件
        mimeData->setData("playerid", player_id_.toUtf8());
        drag->setMimeData(mimeData);
        drag->start(Qt::CopyAction | Qt::MoveAction);
    } else {
        QWidget::mouseMoveEvent(event);
    }
}

void ShowVideoWidget::inputOneFrame(IVideoFrame* videoFrame) {
    FunctionTransfer::runInMainThread([=] () {
        int width = videoFrame->width();
        int height = videoFrame->height();

        if (resolution_width_ <= 0 || resolution_height_ <= 0 || resolution_width_ != width || resolution_height_ != height) {
            setVideoWidth(width, height);
        }
        last_get_frame_time_ = QDateTime::currentMSecsSinceEpoch();
        ReleaseVideoFrame(video_frame_);
        video_frame_ = videoFrame;
        update(); //调用update将执行 paintEvent函数
    });
}

void ShowVideoWidget::initializeGL() {
    qDebug() << __FUNCTION__ << video_frame_;
    is_opengl_inited_ = true;
    initializeOpenGLFunctions();
    glEnable(GL_DEPTH_TEST);
    //现代opengl渲染管线依赖着色器来处理传入的数据
    //着色器：就是使用openGL着色语言(OpenGL Shading Language, GLSL)编写的一个小函数,
    //       GLSL是构成所有OpenGL着色器的语言,具体的GLSL语言的语法需要读者查找相关资料
    //初始化顶点着色器 对象
    vertex_shader_ = new QOpenGLShader(QOpenGLShader::Vertex, this);
    //顶点着色器源码
    const char *vsrc = "attribute vec4 vertexIn; \
    attribute vec2 textureIn; \
    varying vec2 textureOut;  \
    void main(void)           \
    {                         \
        gl_Position = vertexIn; \
        textureOut = textureIn; \
    }";
    //编译顶点着色器程序
    bool bCompile = vertex_shader_->compileSourceCode(vsrc);
    if (!bCompile) {
    }
    //初始化片段着色器 功能gpu中yuv转换成rgb
    fragment_shader_ = new QOpenGLShader(QOpenGLShader::Fragment, this);
    //片段着色器源码(windows下opengl es 需要加上float这句话)
    const char *fsrc =
#if defined(WIN32)
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
#else
#endif
        "varying vec2 textureOut; \
    uniform sampler2D tex_y; \
    uniform sampler2D tex_u; \
    uniform sampler2D tex_v; \
    void main(void) \
    { \
        vec3 yuv; \
        vec3 rgb; \
        yuv.x = texture2D(tex_y, textureOut).r; \
        yuv.y = texture2D(tex_u, textureOut).r - 0.5; \
        yuv.z = texture2D(tex_v, textureOut).r - 0.5; \
        rgb = mat3( 1,       1,         1, \
                    0,       -0.39465,  2.03211, \
                    1.13983, -0.58060,  0) * yuv; \
        gl_FragColor = vec4(rgb, 1); \
    }";
    //将glsl源码送入编译器编译着色器程序
    bCompile = fragment_shader_->compileSourceCode(fsrc);
    if (!bCompile) {
    }
    ///用于绘制矩形
    program_ = new QOpenGLShaderProgram(this);
    program_->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource);
    program_->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource);
    program_->link();
    position_attr_ = program_->attributeLocation("posAttr");
    col_attr_ = program_->attributeLocation("colAttr");
#define PROGRAM_VERTEX_ATTRIBUTE 0
#define PROGRAM_TEXCOORD_ATTRIBUTE 1
    //创建着色器程序容器
    shader_program_ = new QOpenGLShaderProgram;
    //将片段着色器添加到程序容器
    shader_program_->addShader(fragment_shader_);
    //将顶点着色器添加到程序容器
    shader_program_->addShader(vertex_shader_);
    //绑定属性vertexIn到指定位置ATTRIB_VERTEX,该属性在顶点着色源码其中有声明
    shader_program_->bindAttributeLocation("vertexIn", ATTRIB_VERTEX);
    //绑定属性textureIn到指定位置ATTRIB_TEXTURE,该属性在顶点着色源码其中有声明
    shader_program_->bindAttributeLocation("textureIn", ATTRIB_TEXTURE);
    //链接所有所有添入到的着色器程序
    shader_program_->link();
    //激活所有链接
    shader_program_->bind();
    //读取着色器中的数据变量tex_y, tex_u, tex_v的位置,这些变量的声明可以在
    //片段着色器源码中可以看到
    texture_uniform_y_ = shader_program_->uniformLocation("tex_y");
    texture_uniform_u_ = shader_program_->uniformLocation("tex_u");
    texture_uniform_v_ = shader_program_->uniformLocation("tex_v");
    // 顶点矩阵
    const GLfloat vertexVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         -1.0f, 1.0f,
         1.0f, 1.0f,
    };
    memcpy(vertex_vertices_, vertexVertices, sizeof(vertexVertices));
    //纹理矩阵
    static const GLfloat textureVertices[] = {
        0.0f,  1.0f,
        1.0f,  1.0f,
        0.0f,  0.0f,
        1.0f,  0.0f,
    };
    //设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
    glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertex_vertices_);
    //设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
    glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
    //启用ATTRIB_VERTEX属性的数据,默认是关闭的
    glEnableVertexAttribArray(ATTRIB_VERTEX);
    //启用ATTRIB_TEXTURE属性的数据,默认是关闭的
    glEnableVertexAttribArray(ATTRIB_TEXTURE);
    //分别创建y,u,v纹理对象
    texture_y_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture_u_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture_v_ = new QOpenGLTexture(QOpenGLTexture::Target2D);
    texture_y_->create();
    texture_u_->create();
    texture_v_->create();
    //获取返回y分量的纹理索引值
    id_y_ = texture_y_->textureId();
    //获取返回u分量的纹理索引值
    id_u_ = texture_u_->textureId();
    //获取返回v分量的纹理索引值
    id_v_ = texture_v_->textureId();
    glClearColor(0.0, 0.0, 0.0, 0.0);//设置背景色-黑色
}

void ShowVideoWidget::resetGLVertex(int window_W, int window_H) {
    //铺满
    if (resolution_width_ <= 0 || resolution_height_ <= 0 || !AppConfig::gVideoKeepAspectRatio) {
        pic_index_x_ = 0.0;
        pic_index_y_ = 0.0;
        // 顶点矩阵
        const GLfloat vertexVertices[] = {
            -1.0f, -1.0f,
             1.0f, -1.0f,
             -1.0f, 1.0f,
             1.0f, 1.0f,
        };
        memcpy(vertex_vertices_, vertexVertices, sizeof(vertexVertices));
        //纹理矩阵
        static const GLfloat textureVertices[] = {
            0.0f,  1.0f,
            1.0f,  1.0f,
            0.0f,  0.0f,
            1.0f,  0.0f,
        };
        //设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertex_vertices_);
        //设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
        glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
        //启用ATTRIB_VERTEX属性的数据,默认是关闭的
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        //启用ATTRIB_TEXTURE属性的数据,默认是关闭的
        glEnableVertexAttribArray(ATTRIB_TEXTURE);
    } else { //按比例
        int pix_W = window_W;
        int pix_H = resolution_height_ * pix_W / resolution_width_;
        int x = this->width() - pix_W;
        int y = this->height() - pix_H;
        x /= 2;
        y /= 2;

        if (y < 0) {
            pix_H = window_H;
            pix_W = resolution_width_ * pix_H / resolution_height_;
            x = this->width() - pix_W;
            y = this->height() - pix_H;
            x /= 2;
            y /= 2;
        }
        pic_index_x_ = x * 1.0 / window_W;
        pic_index_y_ = y * 1.0 / window_H;
        float index_y = y *1.0 / window_H * 2.0 - 1.0;
        float index_y_1 = index_y * -1.0;
        float index_y_2 = index_y;
        float index_x = x *1.0 / window_W * 2.0 - 1.0;
        float index_x_1 = index_x * -1.0;
        float index_x_2 = index_x;
        const GLfloat vertexVertices[] = {
            index_x_2, index_y_2,
            index_x_1,  index_y_2,
            index_x_2, index_y_1,
            index_x_1,  index_y_1,
        };
        memcpy(vertex_vertices_, vertexVertices, sizeof(vertexVertices));
#if TEXTURE_HALF
        static const GLfloat textureVertices[] = {
            0.0f,  1.0f,
            0.5f,  1.0f,
            0.0f,  0.0f,
            0.5f,  0.0f,
        };
#else
        static const GLfloat textureVertices[] = {
            0.0f,  1.0f,
            1.0f,  1.0f,
            0.0f,  0.0f,
            1.0f,  0.0f,
        };
#endif
        //设置属性ATTRIB_VERTEX的顶点矩阵值以及格式
        glVertexAttribPointer(ATTRIB_VERTEX, 2, GL_FLOAT, 0, 0, vertex_vertices_);
        //设置属性ATTRIB_TEXTURE的纹理矩阵值以及格式
        glVertexAttribPointer(ATTRIB_TEXTURE, 2, GL_FLOAT, 0, 0, textureVertices);
        //启用ATTRIB_VERTEX属性的数据,默认是关闭的
        glEnableVertexAttribArray(ATTRIB_VERTEX);
        //启用ATTRIB_TEXTURE属性的数据,默认是关闭的
        glEnableVertexAttribArray(ATTRIB_TEXTURE);
    }
}

void ShowVideoWidget::resizeGL(int window_W, int window_H) {
    last_get_frame_time_ = QDateTime::currentMSecsSinceEpoch();
    // 防止被零除
    if (window_H == 0) {
        window_H = 1;// 将高设为1
    }
    //设置视口
    glViewport(0, 0, window_W, window_H);
    int x = window_W - ui_->pushButton_close->width() - 22;
    int y = 22;
    ui_->pushButton_close->move(x, y);
    x = 0;
    y = window_H / 2 - ui_->widget_erro->height() / 2;
    ui_->widget_erro->move(x, y);
    ui_->widget_erro->resize(window_W, ui_->widget_erro->height());
    x = 0;
    y = window_H - ui_->widget_name->height() - 6;
    ui_->widget_name->move(x, y);
    ui_->widget_name->resize(window_W, ui_->widget_name->height());
    resetGLVertex(window_W, window_H);
}

void ShowVideoWidget::paintGL() {
    last_get_frame_time_ = QDateTime::currentMSecsSinceEpoch();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (ui_->pushButton_close->isVisible()) {
        if (!is_playing_ || !is_closeable_) {
            ui_->pushButton_close->hide();
        }
    }        

    if (!camera_name_.isEmpty() && is_playing_) {
        ui_->widget_name->show();
        QFontMetrics fontMetrics(ui_->label_name->font());
        int fontSize = fontMetrics.width(camera_name_);//获取之前设置的字符串的像素大小
        QString str = camera_name_;
        if (fontSize > (this->width() / 2)) {
            str = fontMetrics.elidedText(camera_name_, Qt::ElideRight, (this->width() / 2));//返回一个带有省略号的字符串
        }
        ui_->label_name->setText(str);
        ui_->label_name->setToolTip(camera_name_);
    } else {
        ui_->widget_name->hide();
    }

    if (is_playing_ && play_failed_) {
        ui_->widget_erro->show();
    } else {
        ui_->widget_erro->hide();
    }
    ///设置中按比例发生改变 则需要重置x y偏量
    if (current_video_keep_aspect_ratio_ != AppConfig::gVideoKeepAspectRatio) {
        current_video_keep_aspect_ratio_ = AppConfig::gVideoKeepAspectRatio;
        resetGLVertex(this->width(), this->height());
    }
    ///绘制矩形框
    if (is_show_face_rect_ && !face_info_list_.isEmpty()) {
        program_->bind();
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);

        for (int i = 0; i < face_info_list_.size(); i++) {
            FaceInfoNode faceNode = face_info_list_.at(i);
            QRect rect = faceNode.faceRect;
            int window_W = this->width();
            int window_H = this->height();
            int pix_W = rect.width();
            int pix_H = rect.height();
            int x = rect.x();
            int y = rect.y();
            float index_x_1 = x *1.0 / resolution_width_ * 2.0 - 1.0;
            float index_y_1 = 1.0 - (y *1.0 / resolution_height_ * 2.0);
            float index_x_2 = (x + pix_W) * 1.0 / resolution_width_ * 2.0 - 1.0;
            float index_y_2 = index_y_1;
            float index_x_3 = index_x_2;
            float index_y_3 = 1.0 - ((y + pix_H) * 1.0 / resolution_height_ * 2.0);
            float index_x_4 = index_x_1;
            float index_y_4 = index_y_3;
            index_x_1 += pic_index_x_;
            index_x_2 += pic_index_x_;
            index_x_3 += pic_index_x_;
            index_x_4 += pic_index_x_;
            index_y_1 -= pic_index_y_;
            index_y_2 -= pic_index_y_;
            index_y_3 -= pic_index_y_;
            index_y_4 -= pic_index_y_;
            const GLfloat vertices[] = {
                index_x_1, index_y_1,
                index_x_2,  index_y_2,
                index_x_3, index_y_3,
                index_x_4,  index_y_4,
            };
            const GLfloat colors[] = {
                0.47843f, 0.768627f, 0.317647f,
                0.47843f, 0.768627f, 0.317647f,
                0.47843f, 0.768627f, 0.317647f,
                0.47843f, 0.768627f, 0.317647f
            };
            glVertexAttribPointer(position_attr_, 2, GL_FLOAT, GL_FALSE, 0, vertices);
            glVertexAttribPointer(col_attr_, 3, GL_FLOAT, GL_FALSE, 0, colors);
            glLineWidth(2.2f); //设置画笔宽度
            glDrawArrays(GL_LINE_LOOP, 0, 4);
        }
        glDisableVertexAttribArray(1);
        glDisableVertexAttribArray(0);
        program_->release();
    }
    auto videoFrame = video_frame_;

    if (videoFrame != nullptr && videoFrame->format() == IVideoFrame::kPixelFormatYUV420P) {
        uint8_t *m_pBufYuv420p = videoFrame->buffer();

        if (m_pBufYuv420p != NULL) {
            shader_program_->bind();
            //加载y数据纹理
            //激活纹理单元GL_TEXTURE0
            glActiveTexture(GL_TEXTURE0);
            //使用来自y数据生成纹理
            glBindTexture(GL_TEXTURE_2D, id_y_);
            //使用内存中m_pBufYuv420p数据创建真正的y数据纹理
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, resolution_width_, resolution_height_, 0, GL_RED, GL_UNSIGNED_BYTE, m_pBufYuv420p);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            //加载u数据纹理
            glActiveTexture(GL_TEXTURE1);//激活纹理单元GL_TEXTURE1
            glBindTexture(GL_TEXTURE_2D, id_u_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, resolution_width_ / 2, resolution_height_ / 2, 0, GL_RED, GL_UNSIGNED_BYTE, (char*)m_pBufYuv420p + resolution_width_*resolution_height_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            //加载v数据纹理
            glActiveTexture(GL_TEXTURE2);//激活纹理单元GL_TEXTURE2
            glBindTexture(GL_TEXTURE_2D, id_v_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, resolution_width_ / 2, resolution_height_ / 2, 0, GL_RED, GL_UNSIGNED_BYTE, (char*)m_pBufYuv420p + resolution_width_*resolution_height_ * 5 / 4);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            //指定y纹理要使用新值 只能用0,1,2等表示纹理单元的索引，这是opengl不人性化的地方
            //0对应纹理单元GL_TEXTURE0 1对应纹理单元GL_TEXTURE1 2对应纹理的单元
            glUniform1i(texture_uniform_y_, 0);
            //指定u纹理要使用新值
            glUniform1i(texture_uniform_u_, 1);
            //指定v纹理要使用新值
            glUniform1i(texture_uniform_v_, 2);
            //使用顶点数组方式绘制图形
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            shader_program_->release();
        }
    }
}