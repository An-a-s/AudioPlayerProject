#include "AudioPlayer.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle> 
#include <QMessageBox>
#include <QFileInfo>
#include <QTime>
#include <QDir>
#include <QFileDialog> 
#include <QInputDialog> 
#include <QIcon>
#include <QMap>

const QString BASE_PATH = "D:/QuranAudio/";

void AudioPlayer::data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount)
{
    ma_decoder* pDecoder = (ma_decoder*)pDevice->pUserData;
    if (pDecoder == NULL) return;
    ::ma_decoder_read_pcm_frames(pDecoder, pOutput, frameCount, NULL);
    (void)pInput;
}

AudioPlayer::AudioPlayer(QWidget* parent) : QWidget(parent)
{
    currentSurah = nullptr;
    isPlaying = false;
    isLoaded = false;

    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &AudioPlayer::updateProgress);

    setupUi();

    setupDefaultPlaylists();

    updateUiState();

    if (!allPlaylists.isEmpty()) {
        playlistSelector->setCurrentIndex(0);
    }
}

AudioPlayer::~AudioPlayer()
{
    stopClicked();
    for (auto it = allPlaylists.begin(); it != allPlaylists.end(); ++it) {
        deleteList(it.value());
    }
}


void AudioPlayer::setupDefaultPlaylists()
{
    

    if (allPlaylists.isEmpty())
    {

    }
}
void AudioPlayer::addSurahToActiveList(QString name, QString filename, bool isAbsolutePath)
{
    if (!activePlaylist) return;

    SurahNode* newNode = new SurahNode;
    newNode->name = name;
    newNode->path = isAbsolutePath ? filename : (BASE_PATH + filename);
    newNode->next = nullptr;
    newNode->prev = nullptr;

    if (activePlaylist->head == nullptr) {
        activePlaylist->head = newNode;
        activePlaylist->tail = newNode;
    }
    else {
        activePlaylist->tail->next = newNode;
        newNode->prev = activePlaylist->tail;
        activePlaylist->tail = newNode;
    }

    if (activePlaylist->name == playlistSelector->currentText()) {
        playlistWidget->addItem(QString::number(playlistWidget->count() + 1) + ". " + name);
    }
}
void AudioPlayer::deleteList(Playlist& list)
{
    SurahNode* current = list.head;
    while (current != nullptr) {
        SurahNode* nextNode = current->next;
        delete current;
        current = nextNode;
    }
    list.head = nullptr;
    list.tail = nullptr;
}

void AudioPlayer::setupUi()
{
    setWindowTitle("المصحف المرتل");
    resize(750, 650);

    QString cssStyle = R"(
        QWidget { background-color: #1e1e2e; color: #cdd6f4; font-size: 14px; font-family: 'Segoe UI', sans-serif; }
        QListWidget { background-color: #252537; border-radius: 10px; padding: 5px; font-size: 16px; border: 1px solid #303040; }
        QListWidget::item { padding: 8px; border-bottom: 1px solid #303040; }
        QListWidget::item:selected { background-color: #89b4fa; color: #1e1e2e; border-radius: 5px; }
        QPushButton { background-color: #313244; border-radius: 8px; padding: 10px; font-weight: bold; border: 1px solid #45475a; }
        QPushButton:hover { background-color: #45475a; }
        QSlider::groove:horizontal { border: 1px solid #45475a; height: 8px; background: #313244; margin: 2px 0; border-radius: 4px; }
        QSlider::handle:horizontal { background: #89b4fa; width: 18px; height: 18px; border-radius: 9px; margin: -5px 0; }
        QLabel#TimeLabel { font-weight: bold; color: #a6adc8; }
    )";
    setStyleSheet(cssStyle);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QHBoxLayout* selectorLayout = new QHBoxLayout();
    playlistSelector = new QComboBox(this);
    createPlaylistBtn = new QPushButton("إنشاء قائمة جديدة", this);
    createPlaylistBtn->setFixedWidth(150);

    selectorLayout->addWidget(new QLabel("قائمة التشغيل:", this));
    selectorLayout->addWidget(playlistSelector);
    selectorLayout->addWidget(createPlaylistBtn);
    mainLayout->addLayout(selectorLayout);

    connect(playlistSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &AudioPlayer::playlistSelectionChanged);
    connect(createPlaylistBtn, &QPushButton::clicked, this, &AudioPlayer::createNewPlaylistClicked);

    playlistWidget = new QListWidget(this);
    mainLayout->addWidget(playlistWidget);
    connect(playlistWidget, &QListWidget::itemDoubleClicked, this, &AudioPlayer::onPlaylistDoubleClicked);

    statusLabel = new QLabel("القائمة جاهزة", this);
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #fab387;");
    mainLayout->addWidget(statusLabel);

    QHBoxLayout* seekLayout = new QHBoxLayout();
    currentTimeLabel = new QLabel("00:00", this);
    totalTimeLabel = new QLabel("00:00", this);
    seekSlider = new QSlider(Qt::Horizontal, this);
    seekSlider->setRange(0, 100);

    seekLayout->addWidget(currentTimeLabel);
    seekLayout->addWidget(seekSlider);
    seekLayout->addWidget(totalTimeLabel);
    mainLayout->addLayout(seekLayout);

    connect(seekSlider, &QSlider::sliderPressed, this, &AudioPlayer::onSliderPressed);
    connect(seekSlider, &QSlider::sliderReleased, this, &AudioPlayer::onSliderReleased);
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    QLabel* volIcon = new QLabel("🔊", this);
    volumeSlider = new QSlider(Qt::Horizontal, this);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(80);
    volumeSlider->setFixedWidth(100);
    connect(volumeSlider, &QSlider::valueChanged, this, &AudioPlayer::setVolume);

    addBtn = new QPushButton("➕ إضافة سورة", this);
    deleteBtn = new QPushButton("❌ حذف المحدد", this);
    connect(addBtn, &QPushButton::clicked, this, &AudioPlayer::addSurahClicked);
    connect(deleteBtn, &QPushButton::clicked, this, &AudioPlayer::deleteSurahClicked);

    prevBtn = new QPushButton(this);
    prevBtn->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
    playBtn = new QPushButton(this);
    playBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    playBtn->setFixedSize(50, 50);
    playBtn->setIconSize(QSize(32, 32));
    stopBtn = new QPushButton(this);
    stopBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    nextBtn = new QPushButton(this);
    nextBtn->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));

    controlsLayout->addWidget(volIcon);
    controlsLayout->addWidget(volumeSlider);

    controlsLayout->addWidget(addBtn);
    controlsLayout->addWidget(deleteBtn);

    controlsLayout->addStretch();
    controlsLayout->addWidget(prevBtn);
    controlsLayout->addWidget(playBtn);
    controlsLayout->addWidget(stopBtn);
    controlsLayout->addWidget(nextBtn);
    controlsLayout->addStretch();
    controlsLayout->addSpacing(120);

    mainLayout->addLayout(controlsLayout);

    connect(playBtn, &QPushButton::clicked, this, &AudioPlayer::playPauseClicked);
    connect(stopBtn, &QPushButton::clicked, this, &AudioPlayer::stopClicked);
    connect(nextBtn, &QPushButton::clicked, this, &AudioPlayer::nextClicked);
    connect(prevBtn, &QPushButton::clicked, this, &AudioPlayer::prevClicked);
}



void AudioPlayer::createNewPlaylistClicked()
{
    bool ok;
    QString name = QInputDialog::getText(this, "إنشاء قائمة جديدة",
        "أدخل اسم قائمة التشغيل:", QLineEdit::Normal,
        "", &ok);
    if (!ok || name.isEmpty() || allPlaylists.contains(name)) {
        if (allPlaylists.contains(name)) QMessageBox::warning(this, "تنبيه", "هذه القائمة موجودة بالفعل.");
        return;
    }

    QString iconPath = QFileDialog::getOpenFileName(this, "اختر أيقونة للقائمة (اختياري)", "",
        "ملفات الصور (*.png *.jpg *.svg)");

    allPlaylists.insert(name, { name, nullptr, nullptr, iconPath });

    playlistSelector->addItem(QIcon(iconPath), name);
    playlistSelector->setCurrentText(name);
}

void AudioPlayer::playlistSelectionChanged(int index)
{
    if (index < 0) return;

    QString name = playlistSelector->itemText(index);

    if (!allPlaylists.contains(name)) return;

    stopClicked();
    activePlaylist = &allPlaylists[name];
    currentSurah = nullptr;

    playlistWidget->clear();
    SurahNode* current = activePlaylist->head;
    int i = 1;
    while (current != nullptr) {
        playlistWidget->addItem(QString::number(i) + ". " + current->name);
        current = current->next;
        i++;
    }
    statusLabel->setText("تم تحميل قائمة: " + name);
    updateUiState();
}

void AudioPlayer::addSurahClicked()
{
    if (!activePlaylist) {
        QMessageBox::warning(this, "تنبيه", "يجب إنشاء أو اختيار قائمة تشغيل أولاً.");
        return;
    }

    QString filePath = QFileDialog::getOpenFileName(
        this,
        "اختر ملف سورة (MP3)",
        BASE_PATH,
        "ملفات الصوت (*.mp3)"
    );

    if (filePath.isEmpty()) return;

    QFileInfo fileInfo(filePath);
   
    QString baseName = fileInfo.fileName();

   
    SurahNode* current = activePlaylist->head;
    while (current != nullptr) {
        if (current->path == filePath) {
            QMessageBox::warning(this, "تنبيه", "هذا الملف مضاف بالفعل إلى القائمة.");
            return;
        }
        current = current->next;
    }

    
    addSurahToActiveList(baseName, filePath, true);

    QMessageBox::information(this, "نجاح", "تمت إضافة السورة بنجاح.");
}

bool AudioPlayer::deleteSurahFromActiveList(const QString& name)
{
    if (!activePlaylist) return false;

    SurahNode* current = activePlaylist->head;

    while (current != nullptr && current->name != name) {
        current = current->next;
    }

    if (current == nullptr) return false;

    if (current == activePlaylist->head) {
        activePlaylist->head = current->next;
    }

    if (current == activePlaylist->tail) {
        activePlaylist->tail = current->prev;
    }

    if (current->prev != nullptr) {
        current->prev->next = current->next;
    }
    if (current->next != nullptr) {
        current->next->prev = current->prev;
    }

    delete current;
    return true;
}
void AudioPlayer::deleteSurahClicked()
{
    QListWidgetItem* item = playlistWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "تنبيه", "يجب تحديد سورة من القائمة للحذف.");
        return;
    }

    int indexToDelete = playlistWidget->row(item);

    SurahNode* nodeToDelete = activePlaylist->head;
    for (int i = 0; i < indexToDelete; ++i) {
        if (nodeToDelete) {
            nodeToDelete = nodeToDelete->next;
        }
        else {
            QMessageBox::critical(this, "خطأ", "فشل في مزامنة البيانات.");
            return;
        }
    }

    if (nodeToDelete == nullptr) {
        QMessageBox::critical(this, "خطأ", "العقدة غير موجودة في الذاكرة.");
        return;
    }

    QString surahNameInList = nodeToDelete->name; 

    
    if (currentSurah && currentSurah->name == surahNameInList) {
        stopClicked();
        currentSurah = nullptr;
    }

    
    if (deleteSurahFromActiveList(surahNameInList)) {
        delete playlistWidget->takeItem(playlistWidget->row(item));
        QMessageBox::information(this, "نجاح", "تم حذف السورة بنجاح.");

       
        for (int i = 0; i < playlistWidget->count(); ++i) {
            QListWidgetItem* currentItem = playlistWidget->item(i);
            QString oldText = currentItem->text();

            
            QString baseName = oldText.section('.', 1).trimmed();

            currentItem->setText(QString::number(i + 1) + ". " + baseName);
        }
    }
    else {
        QMessageBox::critical(this, "خطأ", "فشل الحذف من القائمة المترابطة (خطأ منطقي).");
    }
}


bool AudioPlayer::loadTrack(SurahNode* node) {
    if (node == nullptr) return false;
    stopClicked();

    currentSurah = node;
    std::wstring wFilePath = currentSurah->path.toStdWString();

    if (::ma_decoder_init_file_w(wFilePath.c_str(), NULL, &audioDecoder) != MA_SUCCESS) {
        QMessageBox::critical(this, "خطأ في الملف", "لم يتم العثور على الملف:\n" + currentSurah->path);
        return false;
    }

    ::ma_decoder_get_length_in_pcm_frames(&audioDecoder, &totalFrames);
    totalTimeLabel->setText(formatTime(totalFrames, audioDecoder.outputSampleRate));
    seekSlider->setRange(0, (int)totalFrames);

    deviceConfig = ::ma_device_config_init(ma_device_type_playback);
    deviceConfig.playback.format = audioDecoder.outputFormat;
    deviceConfig.playback.channels = audioDecoder.outputChannels;
    deviceConfig.sampleRate = audioDecoder.outputSampleRate;
    deviceConfig.dataCallback = data_callback;
    deviceConfig.pUserData = &audioDecoder;

    if (::ma_device_init(NULL, &deviceConfig, &audioDevice) != MA_SUCCESS) {
        ::ma_decoder_uninit(&audioDecoder);
        return false;
    }

    setVolume(volumeSlider->value());
    isLoaded = true;

    QList<QListWidgetItem*> items = playlistWidget->findItems(currentSurah->name, Qt::MatchExactly);
    if (!items.isEmpty()) {
        playlistWidget->setCurrentItem(items.first());
    }

    return true;
}

void AudioPlayer::playPauseClicked() {
    if (!isLoaded) {
        if (activePlaylist->head == nullptr) return;
        loadTrack(activePlaylist->head);
    }

    if (isPlaying) {
        ::ma_device_stop(&audioDevice);
        timer->stop();
        isPlaying = false;
        statusLabel->setText("متوقف: " + currentSurah->name);
    }
    else {
        ::ma_device_start(&audioDevice);
        timer->start(100);
        isPlaying = true;
        statusLabel->setText("تشغيل: " + currentSurah->name);
    }
    updateUiState();
}

void AudioPlayer::stopClicked() {
    if (isLoaded) {
        timer->stop();
        ::ma_device_stop(&audioDevice);
        ::ma_device_uninit(&audioDevice);
        ::ma_decoder_uninit(&audioDecoder);
        isLoaded = false;
        isPlaying = false;
        seekSlider->setValue(0);
        currentTimeLabel->setText("00:00");
    }
    updateUiState();
}

void AudioPlayer::updateProgress() {
    if (isLoaded && isPlaying) {
        ma_uint64 cursor;
        ::ma_decoder_get_cursor_in_pcm_frames(&audioDecoder, &cursor);

        seekSlider->blockSignals(true);
        seekSlider->setValue((int)cursor);
        seekSlider->blockSignals(false);
        currentTimeLabel->setText(formatTime(cursor, audioDecoder.outputSampleRate));

  
        if (cursor >= totalFrames && totalFrames > 0) {
            if (currentSurah && currentSurah->next) {
                nextClicked();
            }
            else {
                stopClicked();
                statusLabel->setText("انتهت القائمة.");
            }
        }
    }
}

void AudioPlayer::seekTo(int value) {
    if (isLoaded) {
        ::ma_decoder_seek_to_pcm_frame(&audioDecoder, (ma_uint64)value);
        currentTimeLabel->setText(formatTime(value, audioDecoder.outputSampleRate));
    }
}

void AudioPlayer::setVolume(int value) {
    if (isLoaded) {
        float volume = value / 100.0f;
        ::ma_device_set_master_volume(&audioDevice, volume);
    }
}

QString AudioPlayer::formatTime(ma_uint64 frames, ma_uint32 sampleRate) {
    if (sampleRate == 0) return "00:00";
    qint64 totalSeconds = frames / sampleRate;
    QTime time(0, 0);
    time = time.addSecs(totalSeconds);
    if (totalSeconds > 3600) return time.toString("h:mm:ss");
    return time.toString("mm:ss");
}

void AudioPlayer::nextClicked() {
    if (currentSurah != nullptr && currentSurah->next != nullptr) {
        if (loadTrack(currentSurah->next)) playPauseClicked();
    }
}

void AudioPlayer::prevClicked() {
    if (currentSurah != nullptr && currentSurah->prev != nullptr) {
        if (loadTrack(currentSurah->prev)) playPauseClicked();
    }
}

void AudioPlayer::onPlaylistDoubleClicked(QListWidgetItem* item) {
    int index = playlistWidget->row(item);
    SurahNode* target = activePlaylist->head;
    for (int i = 0; i < index; i++) {
        if (target && target->next) target = target->next;
        else return;
    }

    if (loadTrack(target)) {
        if (!isPlaying) playPauseClicked();
    }
}

void AudioPlayer::updateUiState() {
    playBtn->setIcon(style()->standardIcon(isPlaying ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

void AudioPlayer::onSliderPressed()
{
    timer->stop();
}

void AudioPlayer::onSliderReleased()
{
    if (isLoaded) {
        bool wasPlaying = isPlaying;
        if (wasPlaying) {
            ::ma_device_stop(&audioDevice);
        }

        int value = seekSlider->value();
        ::ma_decoder_seek_to_pcm_frame(&audioDecoder, (ma_uint64)value);
        currentTimeLabel->setText(formatTime(value, audioDecoder.outputSampleRate));

        if (wasPlaying) {
            ::ma_device_start(&audioDevice);
            timer->start(100);
        }
    }
}