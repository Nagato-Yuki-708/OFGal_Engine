#pragma once
#include <functional>
#include <vector>
#include "SharedTypes.h"		//结构体统一定义头文件，其中包含了所有需要用于系统间通信的结构体
#include "InputEvent.h"

class _EventBus {
    //使用单例模式
public:
    _EventBus(const _EventBus&) = delete;
    _EventBus& operator=(const _EventBus&) = delete;

    static _EventBus& getInstance() {
        static _EventBus instance;
        return instance;
    }
    /*
    ==========================================
    音频系统事件
    ==========================================
    */
    using SoundPlay_Handler = std::function < bool(const char*) >;
    /*
    ==========================================
    文件系统事件
    ==========================================
    */
    using ReadLevelData_Handler = std::function < LevelData(const std::string&) >;
    using WriteLevelData_Handler = std::function < bool(const std::string&, const LevelData&) >;
    using GetProjectStructure_Handler = std::function < ProjectStructure(const char*) >;
    using GetFolderStructure_Handler = std::function < FolderStructure(const char*) >;
    using ReadBMP_Handler = std::function < BMP_Data(const char*) >;
    using WriteBPData_Handler = std::function < bool(const std::string&, const BlueprintData&) >;
    using ReadBPData_Handler = std::function < BlueprintData(const std::string&) >;
    /*
    ==========================================
    输入处理系统事件
    ==========================================
    */
	using InputEvent_Handler = std::function <void(const InputEvent&)>;
    /*
    ==========================================
    渲染系统事件
    ==========================================
    */
    using RenderAndPrint_Handler = std::function < void(const LevelData&, TextureSamplingMethod, int) >;
    using RenderAndPrint_ANISOTROPIC_Handler = std::function < void(const LevelData&, int, int) >;



    void subscribe_SoundPlay(SoundPlay_Handler handler);
    void publish_SoundPlay(const char* sound_path);

    void subscribe_ReadLevelData(ReadLevelData_Handler handler);
    void subscribe_WriteLevelData(WriteLevelData_Handler handler);
    void subscribe_GetProjectStructure(GetProjectStructure_Handler handler);
    void subscribe_GetFolderStructure(GetFolderStructure_Handler handler);
    void subscribe_ReadBMP(ReadBMP_Handler handler);
    void subscribe_ReadBPData(ReadBPData_Handler handler);
    void subscribe_WriteBPData(WriteBPData_Handler handler);
    BMP_Data publish_ReadBMP(const char* filepath);
    void publish_WriteLevelData(const std::string& filepath, const LevelData& data);
    LevelData publish_ReadLevelData(const std::string& filepath);
    ProjectStructure publish_GetProjectStructure(const char* RootDirectory);
    FolderStructure publish_GetFolderStructure(const char* Directory);
    BlueprintData publish_ReadBPData(const std::string& filepath);
    void publish_WriteBPData(const std::string& filepath, const BlueprintData& data);

	void subscribe_InputEvent(InputEvent_Handler handler);
	void publish_InputEvent(const InputEvent& event);
	
    void subscribe_RenderAndPrint(RenderAndPrint_Handler handler);
    void publish_RenderAndPrint(const LevelData& currentLevel, TextureSamplingMethod samplingMethod, int MSAA_Multiple);
    void subscribe_RenderAndPrint_ANISOTROPIC(RenderAndPrint_ANISOTROPIC_Handler handler);
    void publish_RenderAndPrint_ANISOTROPIC(const LevelData& currentLevel, int anisoLevel, int MSAA_Multiple);


private:
    _EventBus() = default;
    ~_EventBus() = default;

    std::vector<ReadLevelData_Handler> handlers_ReadLevelData;
    std::vector<WriteLevelData_Handler> handlers_WriteLevelData;
    std::vector<GetProjectStructure_Handler> handlers_GetProjectStructure;
    std::vector<GetFolderStructure_Handler> handlers_GetFolderStructure;
    std::vector<ReadBMP_Handler> handlers_ReadBMP;
    std::vector<WriteBPData_Handler> handlers_WriteBPData;
    std::vector<ReadBPData_Handler> handlers_ReadBPData;

	std::vector<SoundPlay_Handler> handlers_SoundPlay;  // 所有播放音频事件订阅者，按理来说只有 音频系统 会订阅

	std::vector<InputEvent_Handler> handlers_InputEvent;  //输入事件订阅者存放的容器

    std::vector<RenderAndPrint_Handler> handlers_RenderAndPrint;
    std::vector<RenderAndPrint_ANISOTROPIC_Handler> handlers_RenderAndPrint_ANISOTROPIC;
	// 其它订阅者类型，请像上面一样自行添加
};