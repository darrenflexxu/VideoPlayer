#include "xvideotranscode.h"
#include <QFileDialog>

extern "C" {
#include <libavcodec/avcodec.h>
}

void XVideoTranscode::timerEvent(QTimerEvent* ev) {
  if (!player || player->is_pause())
    return;
  player->Update();
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

XVideoTranscode::XVideoTranscode(QWidget* parent) : QWidget(parent) {
  ui.setupUi(this);
  connect(ui.intputChoose, &QPushButton::pressed, [this]() {
    inputURL = QFileDialog::getOpenFileName(
        nullptr, QStringLiteral("输入文件"),
        "",  // 初始目录
        QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv *.wmv);;") +
            QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;") +
            QStringLiteral("所有文件 (*.*)"));
    ui.inputURL->setText(inputURL);
    player = std::make_shared<XConvertor>();
    player->set_gpu_decode(true);

    if (!player->Open(inputURL.toStdString().c_str())) {
      return;
    }
    auto video_codec = player->GetVideoCodec();

    if (video_codec) {
      ui.inputVideoGroup->setEnabled(true);
      ui.inputVideoWidth->setText(QString("%1").arg(video_codec->width));
      ui.inputVideoHeight->setText(QString("%1").arg(video_codec->height));
      ui.inputVideoBitRate->setText(QString("%1").arg(video_codec->bit_rate));
      ui.inputVideoFrameRate->setText(QString("%1").arg(
          (float)video_codec->framerate.num / (float)video_codec->framerate.den));
      ui.inputVideoCodec->setText(
          QString("%1").arg(avcodec_get_name(video_codec->codec_id)));
    } else {
      ui.inputVideoGroup->setEnabled(false);
      ui.inputVideoWidth->setText("");
      ui.inputVideoHeight->setText("");
      ui.inputVideoBitRate->setText("");
      ui.inputVideoFrameRate->setText("");
      ui.inputVideoCodec->setText("");
    }
    auto audio_codec = player->GetAudioCodec();

    if (audio_codec) {
      ui.inputAudioGroup->setEnabled(true);
      ui.inputAudioBitRate->setText(QString("%1").arg(audio_codec->bit_rate));
      ui.inputAudioSampleRate->setText(
          QString("%1 Hz").arg(audio_codec->sample_rate));
      ui.inputAudioCodec->setText(
          QString("%1").arg(avcodec_get_name(audio_codec->codec_id)));
    } else {
      ui.inputAudioGroup->setEnabled(false);
      ui.inputAudioBitRate->setText("");
      ui.inputAudioSampleRate->setText("");
      ui.inputAudioCodec->setText("");
    }

  });
  connect(ui.outputChoose, &QPushButton::pressed, [this]() {
    outputURL = QFileDialog::getSaveFileName(
        nullptr, QStringLiteral("输出文件"),
        "",  // 初始目录
        QStringLiteral("视频文件 (*.flv *.rmvb *.avi *.MP4 *.mkv *.wmv);;") +
            QStringLiteral("音频文件 (*.mp3 *.wma *.wav);;"));
    ui.outputURL->setText(outputURL);
  });
  connect(ui.trancodeStartButton, &QPushButton::pressed, [this]() {
  });
}

XVideoTranscode::~XVideoTranscode() {
  Close();
}
