#ifndef SHOWVIDEOWIDGET_H
#define SHOWVIDEOWIDGET_H

#include <QWidget>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QOpenGLWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLFunctions>
#include <QOpenGLTexture>
#include <QFile>
#include "base/FunctionTransfer.h"
#include "Interface/IVideoFrame.h"

namespace Ui {
class ShowVideoWidget;
}

struct FaceInfoNode {
    QRect faceRect;
};
///显示视频用的widget（使用OPENGL绘制YUV420P数据）
///这个仅仅是显示视频画面的控件
class ShowVideoWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit ShowVideoWidget(QWidget *parent = 0);
    ~ShowVideoWidget();
    void setPlayerId(QString id) { player_id_ = id; } //用于协助拖拽 区分是哪个窗口
    QString getPlayerId() { return player_id_; }
    void setCloseAble(bool isCloseAble);
    void clear();
    void setIsPlaying(bool value);
    void setPlayFailed(bool value);
    void setCameraName(QString name);
    void setVideoWidth(int w, int h);
    void setShowFaceRect(bool value) { is_show_face_rect_ = value; }
    qint64 getLastGetFrameTime() { return last_get_frame_time_; }
    void inputOneFrame(IVideoFrame* videoFrame);

signals:
    void sig_CloseBtnClick();
    void sig_Drag(QString id_from, QString id_to);

protected:
    void enterEvent(QEvent *event);
    void leaveEvent(QEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void initializeGL() Q_DECL_OVERRIDE;
    void resizeGL(int window_W, int window_H) Q_DECL_OVERRIDE;
    void paintGL() Q_DECL_OVERRIDE;

private:
    void resetGLVertex(int window_W, int window_H);

    bool is_playing_;
    bool play_failed_; //播放失败
    bool is_closeable_; //是否显示关闭按钮
    QString camera_name_;
    qint64 last_get_frame_time_; //上一次获取到帧的时间戳
    ///OPenGL用于绘制图像
    GLuint texture_uniform_y_; //y纹理数据位置
    GLuint texture_uniform_u_; //u纹理数据位置
    GLuint texture_uniform_v_; //v纹理数据位置
    GLuint id_y_; //y纹理对象ID
    GLuint id_u_; //u纹理对象ID
    GLuint id_v_; //v纹理对象ID
    QOpenGLTexture* texture_y_;  //y纹理对象
    QOpenGLTexture* texture_u_;  //u纹理对象
    QOpenGLTexture* texture_v_;  //v纹理对象
    QOpenGLShader *vertex_shader_;  //顶点着色器程序对象
    QOpenGLShader *fragment_shader_;  //片段着色器对象
    QOpenGLShaderProgram *shader_program_; //着色器程序容器
    GLfloat *vertex_vertices_; // 顶点矩阵
    float pic_index_x_; //按比例显示情况下 图像偏移量百分比 (相对于窗口大小的)
    float pic_index_y_; //
    int resolution_width_; //视频分辨率宽
    int resolution_height_; //视频分辨率高
    IVideoFrame* video_frame_ = nullptr;
    QList<FaceInfoNode> face_info_list_;
    bool is_opengl_inited_; //openGL初始化函数是否执行过了
    ///OpenGL用于绘制矩形
    bool is_show_face_rect_;
    GLuint position_attr_;
    GLuint col_attr_;
    QOpenGLShaderProgram *program_;
    bool current_video_keep_aspect_ratio_; //当前模式是否是按比例 当检测到与全局变量不一致的时候 则重新设置openGL矩阵
    QString player_id_;
    Ui::ShowVideoWidget *ui_;
};
#endif // SHOWVIDEOWIDGET_H
