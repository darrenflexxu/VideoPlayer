#include "xvideotranscode.h"
#include <QFileDialog>
#include <QTime>

extern "C" {
#include <libavcodec/avcodec.h>
}

void XVideoTranscode::timerEvent(QTimerEvent* ev) {
  if (!player || player->is_pause())
    return;
  auto pos = player->pos_ms();
  auto total = player->total_ms();
  ui.trancodeProgressBar->setValue((pos * 100) / total);
}
void XVideoTranscode::Close() {
  if (!player) {
    return;
  }
  player->Stop();
}
void XVideoTranscode::Pause() {
  if (!player) {
    return;
  }
  player->Pause(!player->is_pause());
}

void XVideoTranscode::closeEvent(QCloseEvent* ev) {
  Close();
}

void XVideoTranscode::SetInputVideoInfo(
    const std::shared_ptr<XPara>& video_codec) {
  if (video_codec) {
    ui.inputVideoGroup->setEnabled(true);
    ui.inputVideoWidth->setText(QString("%1").arg(video_codec->para->width));
    ui.inputVideoHeight->setText(QString("%1").arg(video_codec->para->height));
    ui.inputVideoBitRate->setText(
        QString("%1").arg(video_codec->para->bit_rate));
    ui.inputVideoFrameRate->setText(
        QString("%1").arg((float)video_codec->para->framerate.num /
                          (float)video_codec->para->framerate.den));
    ui.inputVideoCodec->setText(
        QString("%1").arg(avcodec_get_name(video_codec->para->codec_id)));
    ui.inputTimes->setText(
        QTime::fromMSecsSinceStartOfDay(video_codec->total_ms)
            .toString("hh:mm:ss"));
  } else {
    ui.inputVideoGroup->setEnabled(false);
    ui.inputVideoWidth->setText("");
    ui.inputVideoHeight->setText("");
    ui.inputVideoBitRate->setText("");
    ui.inputVideoFrameRate->setText("");
    ui.inputVideoCodec->setText("");
    ui.inputTimes->setText("");
  }
}

void XVideoTranscode::SetOutputVideoInfo(
    const std::shared_ptr<XPara>& video_para) {
  if (video_para) {
    ui.outputVideoGroup->setEnabled(true);
    auto codec_index = ui.outputVideoCodec->findText(
        avcodec_get_name(video_para->para->codec_id));
    ui.outputVideoCodec->setCurrentIndex(codec_index);
    ui.outputVideoBitRate->setText(
        QString("%1").arg(video_para->para->bit_rate));
  } else {
    ui.outputVideoGroup->setEnabled(false);
    ui.outputVideoCodec->setCurrentIndex(-1);
    ui.outputVideoBitRate->setText("");
  }
}

void XVideoTranscode::SetInputAudioInfo(
    const std::shared_ptr<XPara>& audio_codec) {
  if (audio_codec) {
    ui.inputAudioGroup->setEnabled(true);
    ui.inputAudioBitRate->setText(
        QString("%1").arg(audio_codec->para->bit_rate));
    ui.inputAudioSampleRate->setText(
        QString("%1 Hz").arg(audio_codec->para->sample_rate));
    ui.inputAudioCodec->setText(
        QString("%1").arg(avcodec_get_name(audio_codec->para->codec_id)));
    ui.inputAudioChannels->setText(
        QString("%1").arg(audio_codec->para->ch_layout.nb_channels));
  } else {
    ui.inputAudioGroup->setEnabled(false);
    ui.inputAudioBitRate->setText("");
    ui.inputAudioSampleRate->setText("");
    ui.inputAudioCodec->setText("");
    ui.inputAudioChannels->setText("");
  }
}

void XVideoTranscode::SetOutputAudioInfo(
    const std::shared_ptr<XPara>& audio_para) {
  if (audio_para) {
    ui.outputAudioGroup->setEnabled(true);
    auto codec_index = ui.outputAudioCodec->findText(
        avcodec_get_name(audio_para->para->codec_id));
    ui.outputAudioCodec->setCurrentIndex(codec_index);
  } else {
    ui.outputAudioGroup->setEnabled(false);
    ui.outputAudioCodec->setCurrentIndex(-1);
  }
}

XVideoTranscode::XVideoTranscode(QWidget* parent) : QWidget(parent) {
  ui.setupUi(this);
  connect(ui.intputChoose, &QPushButton::pressed, [this]() {
    inputURL = QFileDialog::getOpenFileName(
        this, QStringLiteral("输入文件"),
        "",  // 初始目录
        QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv *.wmv);;") +
            QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;") +
            QStringLiteral("所有文件 (*.*)"));
    ui.inputURL->setText(inputURL);
  });
  connect(ui.inputURL, &QLineEdit::textChanged, [this](const QString& text) {
    player = std::make_shared<XConvertor>();
    player->set_gpu_decode(true);
    player->set_gpu_encode(true);

    if (!player->Open(text.toStdString().c_str())) {
      return;
    }
    SetInputVideoInfo(inputVideoPara = player->GetVideoCodec());
    SetInputAudioInfo(inputAudioPara = player->GetAudioCodec());
  });
  connect(ui.outputChoose, &QPushButton::pressed, [this]() {
    outputURL =
        QFileDialog::getSaveFileName(this, QStringLiteral("输出文件"),
                                     "",  // 初始目录
                                     QStringLiteral("视频文件 (*.MP4);;") +
                                         QStringLiteral("音频文件 (*.mp3);;"));
    ui.outputURL->setText(outputURL);
  });
  connect(ui.outputURL, &QLineEdit::textChanged, [this](const QString& text) {
    inputVideoPara->para->codec_id = AV_CODEC_ID_H264;

    if (inputAudioPara) {
      inputAudioPara->para->codec_id = AV_CODEC_ID_AAC;
    }
    SetOutputVideoInfo(inputVideoPara);
    SetOutputAudioInfo(inputAudioPara);
  });
  connect(ui.trancodeStartButton, &QPushButton::pressed, [this]() {
    if (outputURL.isEmpty() || !player) {
      return;
    }
    std::map<std::string, std::string> video_opts, audio_opts;
    video_opts["preset"] = "slow";  // 提升质量
    player->Start(outputURL.toStdString().c_str(), inputVideoPara->para,
                  inputVideoPara->time_base,
                  inputAudioPara ? inputAudioPara->para : nullptr,
                  inputAudioPara ? inputAudioPara->time_base : nullptr,
                  video_opts, audio_opts);
  });
  connect(ui.outputVideoBitRate, &QLineEdit::textChanged,
          [this](const QString& text) {
            inputVideoPara->para->bit_rate = text.toUInt();
          });
  ui.outputVideoGroup->setEnabled(false);
  ui.outputAudioGroup->setEnabled(false);
}

XVideoTranscode::~XVideoTranscode() {
  Close();
}
