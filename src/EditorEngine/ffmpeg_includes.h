// ffmpeg_includes.h
// Capa: ENGINE (privado).
//
// ÚNICO lugar donde se incluyen los headers de FFmpeg. FFmpeg es una biblioteca en C,
// por eso los headers van dentro de extern "C" (si no, el enlazador buscaría nombres
// "decorados" de C++ que no existen en las DLLs).
//
// Regla de la arquitectura: este header solo se incluye desde el engine (sus .cpp y sus
// headers INTERNOS, como MediaDecoder.h y GraphRunner.h). Los headers PÚBLICOS del engine
// (MediaProbe.h, FilterGraphBuilder.h, PlaybackEngine.h...), que ve el Controller, no
// exponen tipos de FFmpeg; por eso EditorController compila sin FFmpeg en su ruta de includes.
#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
