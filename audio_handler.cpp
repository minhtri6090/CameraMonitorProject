#include "audio_handler.h"
#include "wifi_manager.h"

#define AUDIO_FILES_COUNT (sizeof(audioFiles)/sizeof(audioFiles[0]))

String audioFiles[] = {
  "/amthanh/xin_chao.mp3",                                  
  "/amthanh/ket_noi_wifi_khong_thanh_cong.mp3",            
  "/amthanh/ket_noi_wifi_thanh_cong.mp3",
  "/amthanh/phat_hien_chuyen_dong.mp3"                     
};

Audio *audio = nullptr;

bool audioInitialized = false;

void initializeSDCard() {
    Serial.println("[SD] Initializing SD Card...");
    
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SD_CS);
    
    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] Card Mount Failed");
        return;
    }
    
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No SD card attached");
        return;
    }
    
    Serial.print("[SD] Card Type: ");
    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    } else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    } else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card Size: %lluMB\n", cardSize);

    Serial.println("[SD] Checking audio files:");
    for (int i = 0; i < AUDIO_FILES_COUNT; i++) {
        File audioFile = SD.open(audioFiles[i]);
        if (audioFile) {
            Serial.printf("[SD] ✓ %s (%u bytes)\n", audioFiles[i].c_str(), audioFile.size());
            audioFile.close();
        } else {
            Serial.printf("[SD] ✗ %s (missing)\n", audioFiles[i].c_str());
        }
    }
    
    Serial.println("[SD] SD Card initialized successfully");
}

void initializeAudio() {
    Serial.println("[AUDIO] Initializing I2S Audio...");

    if (audio == nullptr) {
        audio = new Audio();
        if (audio) {
            audio->setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
            audio->setVolume(21); 
            audioInitialized = true;
            Serial.println("[AUDIO] Audio system initialized");
        } else {
            Serial.println("[AUDIO] Failed to create Audio object");
        }
    }
}

void playAudio(int audioIndex) {
    if (!audioInitialized || !audio) {
        Serial.println("[AUDIO] Audio not initialized");
        return;
    }
    
    if (audioIndex < 0 || audioIndex >= AUDIO_FILES_COUNT) { 
        Serial.println("[AUDIO] Invalid audio index");
        return;
    }
    if (audio->isRunning())audio {
        audio->stopSong();
        delay(100);
    }

    String filePath = audioFiles[audioIndex];
    if (SD.exists(filePath)) {
        Serial.printf("[AUDIO] Playing: %s\n", filePath.c_str());
        bool result = audio->connecttoFS(SD, filePath.c_str());
        if (!result) {
            Serial.printf("[AUDIO] Failed to start playback: %s\n", filePath.c_str());
        }
    } else {
        Serial.printf("[AUDIO] File not found: %s\n", filePath.c_str());
    }
}

void handleAudioLoop() {
    if (audioInitialized && audio) {
        audio->loop();
    }
}

bool isAudioPlaying() {
    return (audioInitialized && audio && audio->isRunning());
}

void stopAudio() {
    if (audioInitialized && audio && audio->isRunning()) {
        audio->stopSong();
    }

}