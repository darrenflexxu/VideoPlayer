#include "xvideotranscode.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QTime>

#include <iostream>
#include <map>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

void XVideoTranscode::timerEvent(QTimerEvent* ev) {
  if (!player || player->is_pause()) {
    return;
  }
  ui.trancodeProgressBar->setValue(player->GetPos() * 100);
  ui.trancodeProgressBar->repaint();
  // 转码结束
  if (running_ && player->IsFinished()) {
    killTimer(timer_id_);
    running_ = false;
    ui.trancodeProgressBar->setValue(100);
    ui.trancodeProgressBar->repaint();
    ui.trancodeStartButton->setEnabled(true);
    std::cout << player->DumpInfo() << std::flush;
    if (player->HasError()) {
      ui.statusLabel->setText(
          QStringLiteral("转码失败: %1").arg(QString::fromStdString(
              player->GetError())));
    } else {
      ui.statusLabel->setText(QStringLiteral("转码完成"));
    }
  }
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

    // 默认输出参数 = 输入参数, 供用户修改
    ui.outputVideoWidth->setText(QString("%1").arg(video_codec->para->width));
    ui.outputVideoHeight->setText(QString("%1").arg(video_codec->para->height));
    ui.outputVideoBitRate->setText(
        QString("%1").arg(video_codec->para->bit_rate));
  } else {
    ui.inputVideoGroup->setEnabled(false);
    ui.inputVideoWidth->setText("");
    ui.inputVideoHeight->setText("");
    ui.inputVideoBitRate->setText("");
    ui.inputVideoFrameRate->setText("");
    ui.inputVideoCodec->setText("");
    ui.inputTimes->setText("");
    ui.outputVideoWidth->setText("");
    ui.outputVideoHeight->setText("");
    ui.outputVideoBitRate->setText("");
  }
}

void XVideoTranscode::SetOutputVideoInfo(
    const std::shared_ptr<XPara>& video_para) {
  bool has_video = !!video_para;
  ui.outputVideoGroup->setEnabled(has_video);
  if (has_video) {
    auto codec_index = ui.outputVideoCodec->findText(
        avcodec_get_name(video_para->para->codec_id));
    if (codec_index < 0) codec_index = 0;  // 默认h264
    ui.outputVideoCodec->setCurrentIndex(codec_index);
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

    ui.outputAudioSampleRate->setText(
        QString("%1").arg(audio_codec->para->sample_rate));
    ui.outputAudioBitRate->setText(
        QString("%1").arg(audio_codec->para->bit_rate));
  } else {
    ui.inputAudioGroup->setEnabled(false);
    ui.inputAudioBitRate->setText("");
    ui.inputAudioSampleRate->setText("");
    ui.inputAudioCodec->setText("");
    ui.inputAudioChannels->setText("");
    ui.outputAudioSampleRate->setText("");
    ui.outputAudioBitRate->setText("");
  }
}

void XVideoTranscode::SetOutputAudioInfo(
    const std::shared_ptr<XPara>& audio_para) {
  bool has_audio = !!audio_para;
  ui.outputAudioGroup->setEnabled(has_audio);
  if (has_audio) {
    auto codec_index = ui.outputAudioCodec->findText(
        avcodec_get_name(audio_para->para->codec_id));
    if (codec_index < 0) codec_index = 0;  // 默认aac
    ui.outputAudioCodec->setCurrentIndex(codec_index);
  }
}

namespace {
// 输出视频编码器(下拉项 -> codec_id)
AVCodecID VideoCodecFromCombo(const QString& text) {
  if (text == "hevc") return AV_CODEC_ID_HEVC;
  return AV_CODEC_ID_H264;
}
// 输出音频编码器(下拉项 -> codec_id)
AVCodecID AudioCodecFromCombo(const QString& text) {
  if (text == "mp3") return AV_CODEC_ID_MP3;
  return AV_CODEC_ID_AAC;
}
}  // namespace

XVideoTranscode::XVideoTranscode(QWidget* parent) : QWidget(parent) {
  ui.setupUi(this);

  ui.cancelButton->setEnabled(false);

  connect(ui.intputChoose, &QPushButton::pressed, [this]() {
    outputURL.clear();
    auto file =
        QFileDialog::getOpenFileName(this, QStringLiteral("打开文件"), "",
                                     QStringLiteral("视频文件 (*.flv *.rmvb "
                                                    "*.avi *.MP4 *.mkv *.wmv)"
                                                    ";;") +
                                         QStringLiteral("音频文件 (*.mp3 "
                                                        "*.aac);;") +
                                         QStringLiteral("所有文件 (*.*)"));
    if (file.isEmpty()) {
      return;
    }
    player = std::make_shared<XConvertor>();
    player->set_gpu_decode(true);
    player->set_gpu_encode(true);

    ui.statusLabel->setText(QStringLiteral("正在打开输入文件..."));
    if (!player->Open(file.toStdString().c_str())) {
      ui.statusLabel->setText(QStringLiteral("打开文件失败"));
      return;
    }
    ui.statusLabel->setText(QStringLiteral("已选择输入文件"));
    SetInputVideoInfo(inputVideoPara = player->GetVideoCodec());
    SetInputAudioInfo(inputAudioPara = player->GetAudioCodec());
  });
  connect(ui.outputChoose, &QPushButton::pressed, [this]() {
    outputURL =
        QFileDialog::getSaveFileName(this, QStringLiteral("保存文件"), "",
                                     QStringLiteral("视频文件 (*.MP4);;") +
                                         QStringLiteral("音频文件 (*.mp3);;"));
    if (outputURL.isEmpty()) {
      return;
    }
    ui.outputURL->setText(outputURL);
  });
  connect(ui.outputURL, &QLineEdit::textChanged, [this](const QString& text) {
    if (text.isEmpty()) {
      return;
    }
    // 根据输出容器预置编码器(comboBox变化会回调刷新codec_id)
    if (QFileInfo(text).suffix().toLower() == "mp3") {
      if (inputVideoPara) {
        ui.statusLabel->setText(
            QStringLiteral("MP3格式不支持视频流, 请改用MP4"));
        ui.outputURL->clear();
        return;
      }
      // 纯音频时自动选择 mp3 编码器
      int mp3_index = ui.outputAudioCodec->findText("mp3");
      if (inputAudioPara && mp3_index >= 0) {
        ui.outputAudioCodec->setCurrentIndex(mp3_index);
      }
    } else {
      SetOutputVideoInfo(inputVideoPara);
      SetOutputAudioInfo(inputAudioPara);
    }
  });
  connect(ui.trancodeStartButton, &QPushButton::pressed, [this]() {
    if (outputURL.isEmpty() || !player || !inputVideoPara && !inputAudioPara) {
      return;
    }
    // 应用输出编码参数
    std::map<std::string, std::string> video_opts;
    std::map<std::string, std::string> audio_opts;
    if (inputVideoPara) {
      inputVideoPara->para->codec_id =
          VideoCodecFromCombo(ui.outputVideoCodec->currentText());
      bool ok = false;
      int w = ui.outputVideoWidth->text().toInt(&ok);
      if (ok && w > 0) inputVideoPara->para->width = w & ~1;  // 取偶数
      int h = ui.outputVideoHeight->text().toInt(&ok);
      if (ok && h > 0) inputVideoPara->para->height = h & ~1;
      long long br = ui.outputVideoBitRate->text().toLongLong(&ok);
      if (ok && br > 0) {
        video_opts["bit_rate"] = std::to_string(br);
      } else {
        // 未指定码率时使用CRF固定质量
        video_opts["preset"] = "medium";
        // x265没有high等高配置, profile只对h264有效
        if (inputVideoPara->para->codec_id != AV_CODEC_ID_HEVC) {
          video_opts["profile"] = "high";
        }
        video_opts["crf"] = "18";
      }
    }
    if (inputAudioPara) {
      inputAudioPara->para->codec_id =
          AudioCodecFromCombo(ui.outputAudioCodec->currentText());
      bool ok = false;
      int rate = ui.outputAudioSampleRate->text().toInt(&ok);
      if (ok && rate > 0) inputAudioPara->para->sample_rate = rate;
      // MP3采样率只支持 22050/32000/44100/48000
      if (inputAudioPara->para->codec_id == AV_CODEC_ID_MP3) {
        int r = inputAudioPara->para->sample_rate;
        if (r != 22050 && r != 32000 && r != 44100 && r != 48000) {
          inputAudioPara->para->sample_rate = 44100;
        }
      }
      long long abr = ui.outputAudioBitRate->text().toLongLong(&ok);
      if (ok && abr > 0) {
        inputAudioPara->para->bit_rate = abr;
      }
    }
    ui.statusLabel->setText(QStringLiteral("正在转码..."));
    ui.trancodeStartButton->setEnabled(false);
    ui.cancelButton->setEnabled(true);
    player->Start(outputURL.toStdString().c_str(),
                  inputVideoPara ? inputVideoPara->para : nullptr,
                  inputVideoPara ? inputVideoPara->time_base : nullptr,
                  inputAudioPara ? inputAudioPara->para : nullptr,
                  inputAudioPara ? inputAudioPara->time_base : nullptr,
                  video_opts, audio_opts);
    if (player->HasError()) {
      ui.statusLabel->setText(
          QStringLiteral("启动失败: %1")
              .arg(QString::fromStdString(player->GetError())));
      ui.trancodeStartButton->setEnabled(true);
      ui.cancelButton->setEnabled(false);
      return;
    }
    running_ = true;
    timer_id_ = startTimer(10);
  });
  connect(ui.cancelButton, &QPushButton::pressed, [this]() {
    if (!player) {
      return;
    }
    ui.statusLabel->setText(QStringLiteral("正在停止..."));
    player->Stop();
    if (running_) {
      killTimer(timer_id_);
      running_ = false;
    }
    ui.trancodeProgressBar->setValue(0);
    ui.cancelButton->setEnabled(false);
    ui.trancodeStartButton->setEnabled(true);
    ui.statusLabel->setText(QStringLiteral("已取消"));
  });
  connect(ui.outputVideoCodec, &QComboBox::currentTextChanged,
          [this](const QString& text) {
            if (!inputVideoPara) {
              return;
            }
            inputVideoPara->para->codec_id = VideoCodecFromCombo(text);
          });
  connect(ui.outputAudioCodec, &QComboBox::currentTextChanged,
          [this](const QString& text) {
            if (!inputAudioPara) {
              return;
            }
            inputAudioPara->para->codec_id = AudioCodecFromCombo(text);
          });

  ui.outputVideoGroup->setEnabled(false);
  ui.outputAudioGroup->setEnabled(false);
  ui.trancodeStartButton->setEnabled(false);
}

XVideoTranscode::~XVideoTranscode() {
  Close();
}
