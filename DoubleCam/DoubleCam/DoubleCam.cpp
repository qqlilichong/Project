// DoubleCam.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////

int decode_video_packet( Cw2FFmpegAVCodecContextOpen& vdecoder_cam,
						Cw2FFmpegAVFrame& frame_video_cam,

						Cw2FFmpegAVCodecContextOpen& vdecoder_audio,
						Cw2FFmpegAVFrame& frame_audio,
						
						AVPacket& pkt, int& got_frame )
{
	int decoded = pkt.size ;
	//auto video_stream_index = (int)vdecoder_cam->opaque ;
	auto audio_stream_index = (int)vdecoder_audio->opaque ;

// 	if ( video_stream_index == pkt.stream_index )
// 	{
// 		auto ret = avcodec_decode_video2( vdecoder_cam, frame_video_cam, &got_frame, &pkt ) ;
// 		if ( ret < 0 )
// 		{
// 			return ret ;
// 		}
// 	}
	
	if ( audio_stream_index == pkt.stream_index )
	{
		auto ret = avcodec_decode_audio4( vdecoder_audio, frame_audio, &got_frame, &pkt ) ;
		if ( ret < 0 )
		{
			return ret ;
		}
		
		decoded = FFMIN( ret, pkt.size ) ;
	}

	return decoded ;
}

static char *dup_wchar_to_utf8(wchar_t *w)
{
	char *s = NULL;
	int l = WideCharToMultiByte(CP_UTF8, 0, w, -1, 0, 0, 0, 0);
	s = (char *) av_malloc(l);
	if (s)
		WideCharToMultiByte(CP_UTF8, 0, w, -1, s, l, 0, 0);
	return s;
}

class Cw2UserOSD : public IW2_FFUSERSINK
{
public:
	virtual int OnDrawPadDC( HDC dc, D3DSURFACE_DESC desc )
	{
		return 0 ;
	}
};

int _tmain( int argc, _TCHAR* argv[] )
{
	WSL2_FFmpegInit() ;

	if ( true )
	{
		const char* output_file = "test.mp4" ;
		const int video_width = 352 ;
		const int video_height = 288 ;

		const int pad_width = video_width ;
		const int pad_height = video_height * 2 ;

		RECT rcViewA = { 0, 0, pad_width, pad_height / 2 } ;
		RECT rcViewB = { 0, pad_height / 2, pad_width, pad_height } ;

		HWND hViewA = (HWND)0x0029033E ;
		HWND hViewB = (HWND)0x0029033E ;
		
		Cw2D3D9 d3d9 ;
		Cw2D3D9Device d3dev ;
		if ( d3dev.CreateDevice( d3d9, GetDesktopWindow(), D3DADAPTER_DEFAULT, nullptr, pad_width, pad_height ) != 0 )
		{
			return 0 ;
		}
		
		static Cw2UserOSD osd ;

		Cw2DrawPad pad ;
		pad.SetUserSink( &osd ) ;

		Cw2DrawWnd dw1 ;
		dw1.AddSink( &pad ) ;
		if ( dw1.Init( hViewA, rcViewA, &d3dev ) != 0 )
		{
			return 0 ;
		}

		Cw2DrawWnd dw2 ;
		dw2.AddSink( &pad ) ;
		if ( dw2.Init( hViewB, rcViewB, &d3dev ) != 0 )
		{
			return 0 ;
		}

		map< string, string > cam_options ;
		cam_options[ "video_size" ] = WSL2_String_FormatA( "%dx%d", video_width, video_height ) ;
		cam_options[ "pixel_format" ] = "yuyv422" ;

		Cw2OpenCam cam1 ;
		cam1.AddSink( &dw1 ) ;
		if ( cam1.OpenCam( "@device_pnp_\\\\?\\usb#vid_0ac8&pid_332d&mi_00#7&3a816f4f&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global", &cam_options ) == 0 )
		{
			pad.AddViewRect( rcViewA ) ;
		}

		Cw2OpenCam cam2 ;
		cam2.AddSink( &dw2 ) ;
		if ( cam2.OpenCam( "USB2.0 PC CAMERA", &cam_options ) == 0 )
		{
			pad.AddViewRect( rcViewB ) ;
		}

		Cw2OpenAudioCapture audiocap ;
		if ( audiocap.OpenAudioCapture( "麦克风 (Realtek High Definition Au", nullptr ) == 0 )
		{
			audiocap.AddSink( &pad ) ;
		}

		system("pause");

		shared_ptr< Cw2FileEncoder > file_encoder_ptr ;
		
		if ( !file_encoder_ptr )
		{
			try
			{
				file_encoder_ptr.reset( new Cw2FileEncoder ) ;
				if ( !file_encoder_ptr )
				{
					throw -1 ;
				}

				if ( file_encoder_ptr->OpenFile( output_file ) != 0 )
				{
					throw -1 ;
				}
				
				map< string, string > video_options ;
				video_options[ "preset" ] = "ultrafast" ;
				video_options[ "tune" ] = "zerolatency" ;
				if ( file_encoder_ptr->AddVideoStream( pad_width, pad_height, 800000, &video_options ) != 0 )
				{
					throw -1 ;
				}

				if ( file_encoder_ptr->AddAudioStream( audiocap.GetAudioDecoder(), 96000, nullptr ) != 0 )
				{
					throw -1 ;
				}

				if ( file_encoder_ptr->OpenStreamIO( output_file ) != 0 )
				{
					throw -1 ;
				}

				throw 0 ;
			}

			catch ( int ec )
			{
				if ( ec == 0 )
				{
					pad.AddSink( &(*file_encoder_ptr) ) ;
				}
			}
		}

		system("pause");

		{
			pad.RemoveSink( &(*file_encoder_ptr) ) ;
			file_encoder_ptr.reset() ;
		}

		system("pause");
		
		audiocap.CloseAudioCapture() ;
		cam2.CloseCam() ;
		cam1.CloseCam() ;
		
		return 0 ;
	}
	
	HWND wnd_cam1 = (HWND)0x000602C6 ;
	const char* output = "test.aac" ;

	Cw2FFmpegAVFormatContext av_format_cam1 = avformat_alloc_context() ;
	Cw2FFmpegAVFormatContext av_format_cam2 = avformat_alloc_context() ;

	try
	{
		if ( av_format_cam1.InvalidHandle() )
		{
			throw -1 ;
		}
		
		auto if_dshow_ptr = av_find_input_format( "dshow" ) ;
		if ( if_dshow_ptr == nullptr )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVDictionary options ;
		//av_dict_set( options, "video_size", "640x480", 0 ) ;
		//av_dict_set( options, "framerate", "30", 0 ) ;
		//av_dict_set( options, "pixel_format", "yuyv422", 0 ) ;
		
// 		char* audio_id = dup_wchar_to_utf8( L"video=@device_pnp_\\\\?\\usb#vid_0ac8&pid_332d&mi_00#7&3a816f4f&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global"
// 			L":audio=麦克风 (Realtek High Definition Au" ) ;

		char* audio_id = dup_wchar_to_utf8( L"audio=麦克风 (Realtek High Definition Au" ) ;
		
		if ( avformat_open_input( av_format_cam1, audio_id, if_dshow_ptr, nullptr ) != 0 )
		{
			
			throw -1 ;
		}
		
		if ( avformat_find_stream_info( av_format_cam1, nullptr ) < 0 )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVCodecContextOpen decoder_cam1 ;
		Cw2FFmpegAVCodecContextOpen decoder_audio ;
		for ( decltype( av_format_cam1->nb_streams ) i = 0 ; i < av_format_cam1->nb_streams ; ++i )
		{
			if ( av_format_cam1->streams[ i ]->codec->codec_type == AVMEDIA_TYPE_VIDEO )
			{
				if ( decoder_cam1.InvalidHandle() )
				{
					decoder_cam1 = av_format_cam1->streams[ i ]->codec ;
					decoder_cam1->opaque = (void*)i ;
				}
			}

			else if ( av_format_cam1->streams[ i ]->codec->codec_type == AVMEDIA_TYPE_AUDIO )
			{
				if ( decoder_audio.InvalidHandle() )
				{
					decoder_audio = av_format_cam1->streams[ i ]->codec ;
					decoder_audio->opaque = (void*)i ;
				}
			}
		}

// 		if ( decoder_cam1.InvalidHandle() || decoder_audio.InvalidHandle() )
// 		{
// 			throw -1 ;
// 		}
// 		
// 		if ( avcodec_open2( decoder_cam1, avcodec_find_decoder( decoder_cam1->codec_id ), nullptr ) != 0 )
// 		{
// 			throw -1 ;
// 		}

		if ( avcodec_open2( decoder_audio, avcodec_find_decoder( decoder_audio->codec_id ), nullptr ) != 0 )
		{
			throw -1 ;
		}

// 		auto cam1_avctx_tb = decoder_cam1->time_base ;
// 		auto cam1_stream_tb = av_format_cam1->streams[ 0 ]->time_base ;
// 
// 		Cw2FFmpegSws sws_cam1 ;
// 		if ( !sws_cam1.Init( decoder_cam1->pix_fmt, AV_PIX_FMT_YUV420P, decoder_cam1->coded_width, decoder_cam1->coded_height ) )
// 		{
// 			throw -1 ;
// 		}
// 		
		Cw2FFmpegPictureFrame frame_video_yuv_cam1 ;
// 		frame_video_yuv_cam1.Init( sws_cam1.m_dst.pix_fmt, sws_cam1.m_dst.width, sws_cam1.m_dst.height ) ;
		Cw2FFmpegAVFrame frame_video_cam1 = av_frame_alloc() ;
// 		
// 		CwRenderDirectDraw render_cam1 ;
// 		if ( !render_cam1.InitMediaRender( wnd_cam1, NULL ) )
// 		{
// 			throw -1 ;
// 		}
// 		
// 		CwSurfaceDirectDraw surface_cam1 ;
// 		if ( surface_cam1.InitSurface( sws_cam1.m_dst.width, sws_cam1.m_dst.height, 8,
// 			IMediaSurface::MSRS_TYPE_YV12, &render_cam1 ) != 0 )
// 		{
// 			throw -1 ;
// 		}

		//////////////////////////////////////////////////////////////////////////

 		Cw2FFmpegAVFormatContext av_format_file = avformat_alloc_context() ;
 		av_format_file->oformat = av_guess_format( nullptr, output, nullptr ) ;
		if ( av_format_file->oformat == nullptr )
 		{
			throw -1 ;
 		}

		auto codec_aac = avcodec_find_encoder( AV_CODEC_ID_AAC ) ;

		auto audio_stream = avformat_new_stream( av_format_file, codec_aac ) ;
		if ( audio_stream == nullptr )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVCodecContextOpen encoder_audio = audio_stream->codec ;
		encoder_audio->channels = decoder_audio->channels ;
		encoder_audio->channel_layout = av_get_default_channel_layout( encoder_audio->channels ) ;
		encoder_audio->sample_rate = decoder_audio->sample_rate ;
		encoder_audio->sample_fmt = decoder_audio->sample_fmt ;
		encoder_audio->bit_rate = 96000 ;
		encoder_audio->flags |= AV_CODEC_FLAG_GLOBAL_HEADER ;

		audio_stream->time_base.num = 1 ;
		audio_stream->time_base.den = encoder_audio->sample_rate ;

		if ( avcodec_open2( encoder_audio, avcodec_find_encoder( encoder_audio->codec_id ), nullptr ) != 0 )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVFrame frame_audio = av_frame_alloc() ;
      
//     size = av_samples_get_buffer_size(NULL, pCodecCtx->channels,pCodecCtx->frame_size,pCodecCtx->sample_fmt, 1);  
//     frame_buf = (uint8_t *)av_malloc(size);  
//     avcodec_fill_audio_frame(pFrame, pCodecCtx->channels, pCodecCtx->sample_fmt,(const uint8_t*)frame_buf, size, 1); 

// 
// 		pCodecCtx->codec_id = fmt->audio_codec;  
// 		pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;  
//     pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;  
//     pCodecCtx->sample_rate= 44100;  
//     pCodecCtx->channel_layout=AV_CH_LAYOUT_STEREO;  
//     pCodecCtx->channels = av_get_channel_layout_nb_channels(pCodecCtx->channel_layout);  
//     pCodecCtx->bit_rate = 64000;  
 
// 		auto video_stream = avformat_new_stream( av_format_file, 0 ) ;
// 		if ( video_stream == nullptr )
// 		{
// 			throw -1 ;
// 		}
// 		
// 		Cw2FFmpegAVCodecContextOpen encoder_video = video_stream->codec ;
// 		encoder_video->codec_id = AV_CODEC_ID_H264 ;
// 		encoder_video->codec_type = AVMEDIA_TYPE_VIDEO ;
// 		encoder_video->pix_fmt = sws_cam1.m_dst.pix_fmt ;
// 		encoder_video->width = sws_cam1.m_dst.width ;
// 		encoder_video->height = sws_cam1.m_dst.height ;
// 		encoder_video->time_base.den = 30 ;
// 		encoder_video->time_base.num = 1 ;
// 		encoder_video->bit_rate = 900000 ;
// 		encoder_video->gop_size = 12 ;
// 		encoder_video->me_range = 16 ;
// 		encoder_video->max_qdiff = 4 ;
// 		encoder_video->qcompress = 0.6f ;
// 		encoder_video->qmin = 10 ;
// 		encoder_video->qmax = 51 ;
// 		encoder_video->max_b_frames = 3 ;
// 		encoder_video->flags |= CODEC_FLAG_GLOBAL_HEADER ;
// 
// 		video_stream->time_base.den = 30 ;
// 		video_stream->time_base.num = 1 ;
// 		
// 		Cw2FFmpegAVDictionary enc_options ;
// 		if ( encoder_video->codec_id == AV_CODEC_ID_H264 )
// 		{
// 			av_dict_set( enc_options, "preset", "ultrafast", 0 ) ;
// 			av_dict_set( enc_options, "tune", "zerolatency", 0 ) ;
// 		}
// 
// 		if ( avcodec_open2( encoder_video, avcodec_find_encoder( encoder_video->codec_id ), enc_options ) != 0 )
// 		{
// 			throw -1 ;
// 		}
// 
		//audio_stream->time_base = encoder_audio->time_base ;

		Cw2FFmpegAVIOAuto io_file ;
		io_file.InitIO( av_format_file, output ) ;
		if ( !io_file.IsReady() )
		{
			throw -1 ;
		}
		
		AVAudioFifo* fifo_audio = av_audio_fifo_alloc( encoder_audio->sample_fmt, encoder_audio->channels, 1 ) ;

		//////////////////////////////////////////////////////////////////////////

		Cw2TickCount tw ;
		static int64_t pts = 0 ;
		
		AVPacket pkt ;
		av_init_packet( &pkt ) ;
		pkt.data = nullptr ;
		pkt.size = 0 ;
		while ( av_read_frame( av_format_cam1, &pkt ) >= 0 )
		{
			AVPacket orig_pkt = pkt ;

			do
			{
				int got_frame = 0 ;
				auto ret = decode_video_packet(
					decoder_cam1,
					frame_video_cam1,
					decoder_audio,
					frame_audio,
					pkt, got_frame ) ;
				if ( ret < 0 )
				{
					break ;
				}

				if ( true )
				{
					if ( av_audio_fifo_realloc( fifo_audio, frame_audio->nb_samples ) < 0 )
					{
						break ;
					}

					if ( av_audio_fifo_write( fifo_audio, (void**)frame_audio->data, frame_audio->nb_samples ) != frame_audio->nb_samples )
					{
						break ;
					}
					
					while ( av_audio_fifo_size( fifo_audio ) >= encoder_audio->frame_size )
					{
						Cw2FFmpegAVFrame layout_frame_audio = av_frame_alloc() ;
						layout_frame_audio->nb_samples = encoder_audio->frame_size ;
						layout_frame_audio->channel_layout = encoder_audio->channel_layout ;
						layout_frame_audio->format = encoder_audio->sample_fmt ;
						layout_frame_audio->sample_rate = encoder_audio->sample_rate ;
						av_frame_get_buffer( layout_frame_audio, 0 ) ;

						if ( av_audio_fifo_read( fifo_audio, (void**)layout_frame_audio->data, layout_frame_audio->nb_samples ) != layout_frame_audio->nb_samples )
						{
							int bad = 0 ;
						}

						AVPacket enc_pkt ;
						av_init_packet( &enc_pkt ) ;
						enc_pkt.data = nullptr ;
						enc_pkt.size = 0 ;

						layout_frame_audio->pts = pts ;
						pts += layout_frame_audio->nb_samples ;
							
						int enc_ok = 0 ;
						avcodec_encode_audio2( encoder_audio, &enc_pkt, layout_frame_audio, &enc_ok ) ;
						if ( enc_ok )
						{
							enc_pkt.stream_index = 0 ;
							io_file.WritePacket( enc_pkt ) ;
						}
							
						av_free_packet( &enc_pkt ) ;
					}

// 				if ( got_frame )
// 				{
// 					if ( sws_cam1.Scale( frame_video_cam1->data, frame_video_cam1->linesize,
// 						frame_video_yuv_cam1.pic.data, frame_video_yuv_cam1.pic.linesize ) )
// 					{
// 						RenderCam1( wnd_cam1, render_cam1, surface_cam1, frame_video_yuv_cam1.pic ) ;
// 
// 						if ( true )
// 						{
// 							AVPacket enc_pkt ;
// 							av_init_packet( &enc_pkt ) ;
// 							enc_pkt.data = nullptr ;
// 							enc_pkt.size = 0 ;
// 
// 							AVFrame enc_frame = { 0 } ;
// 							enc_frame.format = frame_video_yuv_cam1.ctx.pix_fmt ;
// 							enc_frame.width = frame_video_yuv_cam1.ctx.width ;
// 							enc_frame.height = frame_video_yuv_cam1.ctx.height ;
// 							enc_frame.data[ 0 ] = frame_video_yuv_cam1.pic.data[ 0 ] ;
// 							enc_frame.data[ 1 ] = frame_video_yuv_cam1.pic.data[ 1 ] ;
// 							enc_frame.data[ 2 ] = frame_video_yuv_cam1.pic.data[ 2 ] ;
// 							enc_frame.linesize[ 0 ] = frame_video_yuv_cam1.pic.linesize[ 0 ] ;
// 							enc_frame.linesize[ 1 ] = frame_video_yuv_cam1.pic.linesize[ 1 ] ;
// 							enc_frame.linesize[ 2 ] = frame_video_yuv_cam1.pic.linesize[ 2 ] ;
// 							enc_frame.pts = 0 ;
// 							
// 							int enc_ok = 0 ;
// 							avcodec_encode_video2( encoder_video, &enc_pkt, &enc_frame, &enc_ok ) ;
// 							if ( enc_ok )
// 							{
// 								enc_pkt.stream_index = video_stream->index ;
// 								io_file.WritePacket( enc_pkt ) ;
// 							}
// 							
// 							av_free_packet( &enc_pkt ) ;
// 						}
// 					}
// 				}
				}

				pkt.data += ret ;
				pkt.size -= ret ;

			} while ( pkt.size > 0 ) ;
			
			av_free_packet( &orig_pkt ) ;

			if ( tw.TickNow() > 20.0f )
			{
				break ;
			}
		}
		
		av_audio_fifo_free( fifo_audio ) ;
		int end = 0 ;
	}

	catch( ... )
	{
		
	}

	avformat_close_input( av_format_cam1 ) ;
	avformat_close_input( av_format_cam2 ) ;
	return 0 ;
}

//////////////////////////////////////////////////////////////////////////
