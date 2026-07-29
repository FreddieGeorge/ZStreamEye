set(ZSTREAMEYE_CORE_SOURCES
    analysis/AnalysisStats.h
    analysis/AnalysisStats.cpp
    analysis/BitstreamSearch.h
    analysis/BitstreamSearch.cpp
    analysis/PlaybackMetrics.h
    analysis/PlaybackMetrics.cpp
    analysis/AccessUnitAnalyzer.h
    analysis/AccessUnitAnalyzer.cpp
    buffering/RebufferState.h
    decode/DecodedFrameAnalysisBuilder.h
    decode/DecodedFrameAnalysisBuilder.cpp
    decode/DecodeEventSink.h
    decode/DecodedFrameDispatcher.h
    decode/DecodedFrameDispatcher.cpp
    decode/DecodeLoop.h
    decode/DecodeLoop.cpp
    decode/DecodeSeekPlanner.h
    decode/DecodeSeekPlanner.cpp
    decode/DecodeWorker.h
    decode/DecodeWorker.cpp
    decode/FFmpegDecoder.h
    decode/FFmpegDecoder.cpp
    decode/FFmpegStreamInfoBuilder.h
    decode/FFmpegStreamInfoBuilder.cpp
    decode/FirstFramePauseController.h
    decode/FirstFramePauseController.cpp
    decode/FramePacing.h
    decode/FramePacing.cpp
    decode/PacketRawDataBuilder.h
    decode/PacketRawDataBuilder.cpp
    decode/PendingAccessUnitDispatcher.h
    decode/PendingAccessUnitDispatcher.cpp
    decode/RebufferProgressTracker.h
    decode/RebufferProgressTracker.cpp
    decode/SeekCheckpointEmitter.h
    decode/SeekCheckpointEmitter.cpp
    decode/StreamLogFormatter.h
    decode/StreamLogFormatter.cpp
    export/AnalysisExportWriter.h
    export/AnalysisExportWriter.cpp
    export/H264SyntaxJsonWriter.h
    export/H264SyntaxJsonWriter.cpp
    model/CodecKind.h
    model/DecodedVideoFrame.h
    model/FrameAnalysis.h
    model/FrameAnalysis.cpp
    model/FrameSeekCheckpoint.h
    model/MediaTypes.h
    model/MediaTypes.cpp
    model/StreamInfo.h
    model/StreamDocument.h
    model/StreamDocument.cpp
    parser/BitstreamParser.h
    parser/BitstreamParser.cpp
    parser/ParserFactory.h
    parser/ParserFactory.cpp
    parser/audio/AacAdtsParser.h
    parser/audio/AacAdtsParser.cpp
    parser/audio/Mp3FrameParser.h
    parser/audio/Mp3FrameParser.cpp
    parser/video/h264/cabac/H264CabacContextModel.h
    parser/video/h264/cabac/H264CabacContextModel.cpp
    parser/video/h264/cabac/H264CabacDecoder.h
    parser/video/h264/cabac/H264CabacDecoder.cpp
    parser/video/h264/cabac/H264CabacMacroblockParser.h
    parser/video/h264/cabac/H264CabacMacroblockParser.cpp
    parser/video/h264/cabac/H264CabacMacroblockSyntaxReader.h
    parser/video/h264/cabac/H264CabacMacroblockSyntaxReader.cpp
    parser/video/h264/cabac/H264CabacResidualSyntaxReader.h
    parser/video/h264/cabac/H264CabacResidualSyntaxReader.cpp
    parser/video/h264/cabac/H264CabacSubMacroblockSyntaxReader.h
    parser/video/h264/cabac/H264CabacSubMacroblockSyntaxReader.cpp
    parser/video/h264/cabac/H264CabacSyntaxReader.h
    parser/video/h264/cabac/H264CabacSyntaxTypes.h
    parser/video/h264/cavlc/H264CavlcMacroblockParser.h
    parser/video/h264/cavlc/H264CavlcMacroblockParser.cpp
    parser/video/h264/cavlc/H264CavlcMacroblockResidualParser.h
    parser/video/h264/cavlc/H264CavlcMacroblockResidualParser.cpp
    parser/video/h264/cavlc/H264CavlcResidualParser.h
    parser/video/h264/cavlc/H264CavlcResidualParser.cpp
    parser/video/h264/H264FrameAnalysisAdapter.h
    parser/video/h264/H264FrameAnalysisAdapter.cpp
    parser/video/h264/H264MacroblockParser.cpp
    parser/video/h264/H264MacroblockTypes.h
    parser/video/h264/H264MacroblockTypes.cpp
    parser/video/h264/H264MotionVectorParser.h
    parser/video/h264/H264MotionVectorParser.cpp
    parser/video/h264/H264ParameterSetParser.cpp
    parser/video/h264/H264Parser.h
    parser/video/h264/H264Parser.cpp
    parser/video/h264/H264SliceDataContext.h
    parser/video/h264/H264SliceHeaderParser.cpp
    parser/video/hevc/HevcParser.h
    parser/video/hevc/HevcParser.cpp
    util/BitReader.h
    util/BitReader.cpp
    util/ByteStream.h
    util/ByteStream.cpp
    util/Rbsp.h
    util/Rbsp.cpp
)

add_library(ZStreamEyeCore STATIC ${ZSTREAMEYE_CORE_SOURCES})

target_include_directories(ZStreamEyeCore PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(ZStreamEyeCore PUBLIC
    Qt6::Core
)

if(TARGET PkgConfig::FFMPEG)
    target_link_libraries(ZStreamEyeCore PUBLIC PkgConfig::FFMPEG)
else()
    target_link_libraries(ZStreamEyeCore PUBLIC FFmpeg::FFmpeg)
endif()

target_compile_features(ZStreamEyeCore PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(ZStreamEyeCore PRIVATE /utf-8)
endif()
