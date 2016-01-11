// DoubleCam.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////

// int decode_video_packet( Cw2FFmpegAVCodecContextOpen& vdecoder_cam, Cw2FFmpegAVFrame& frame_video_cam, AVPacket& pkt, int& got_frame )
// {
// 
// 		
// 	int decoded = pkt.size ;
// 	auto video_stream_index = (int)vdecoder_cam->opaque ;
// 
// 	if ( video_stream_index == pkt.stream_index )
// 	{
// 		auto ret = avcodec_decode_video2( vdecoder_cam, frame_video_cam, &got_frame, &pkt ) ;
// 		if ( ret < 0 )
// 		{
// 			return ret ;
// 		}
// 
// 		if ( got_frame )
// 		{
// 
// 		}
// 	}
// 
// 	return decoded ;
// }

int _tmain( int argc, _TCHAR* argv[] )
{
	WSL2_FFmpegInit() ;

	if ( true )
	{
		const char* output_file = "test.mp4" ;
		const int video_width = 640 ;
		const int video_height = 480 ;

		const int pad_width = video_width ;
		const int pad_height = video_height * 2 ;

		RECT rcViewA = { 0, 0, pad_width, pad_height / 2 } ;
		RECT rcViewB = { 0, pad_height / 2, pad_width, pad_height } ;

		HWND hViewA = (HWND)0x002A0310 ;
		HWND hViewB = (HWND)0x002204F2 ;
		
		Cw2D3D9 d3d9 ;
		Cw2D3D9Device d3dev ;
		if ( d3dev.CreateDevice( d3d9, GetDesktopWindow(), D3DADAPTER_DEFAULT, nullptr, pad_width, pad_height ) != 0 )
		{
			return 0 ;
		}

		Cw2DrawPad pad ;

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

		Cw2OpenCam cam1 ;
		cam1.AddSink( &dw1 ) ;
		if ( cam1.OpenCam( "@device_pnp_\\\\?\\usb#vid_0ac8&pid_332d&mi_00#7&3a816f4f&0&0000#{65e8773d-8f56-11d0-a3b9-00a0c9223196}\\global", video_width, video_height ) == 0 )
		{
			pad.AddViewRect( rcViewA ) ;
		}

		Cw2OpenCam cam2 ;
		cam2.AddSink( &dw2 ) ;
		if ( cam2.OpenCam( "USB2.0 PC CAMERA", video_width, video_height ) == 0 )
		{
			pad.AddViewRect( rcViewB ) ;
		}

		system("pause");

		shared_ptr< Cw2FileEncoder > file_encoder_ptr ;

		if ( !file_encoder_ptr )
		{
			file_encoder_ptr.reset( new Cw2FileEncoder ) ;
			if ( file_encoder_ptr->Init( output_file, pad_width, pad_height ) == 0 )
			{
				pad.AddSink( &(*file_encoder_ptr) ) ;
			}
		}

		system("pause");

		{
			pad.RemoveSink( &(*file_encoder_ptr) ) ;
			file_encoder_ptr->Uninit() ;
			file_encoder_ptr.reset() ;
		}

		system("pause");

		cam2.CloseCam() ;
		cam1.CloseCam() ;

		return 0 ;
	}

	/*
	HWND wnd_cam1 = (HWND)0x000602C6 ;
	const char* output = "test.mp4" ;

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
		av_dict_set( options, "video_size", "640x480", 0 ) ;
		//av_dict_set( options, "framerate", "30", 0 ) ;
		av_dict_set( options, "pixel_format", "yuyv422", 0 ) ;
		if ( avformat_open_input( av_format_cam1, "video=Vega USB 2.0 Camera.", if_dshow_ptr, nullptr ) != 0 )
		{
			throw -1 ;
		}
		
		if ( avformat_find_stream_info( av_format_cam1, nullptr ) < 0 )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVCodecContextOpen decoder_cam1 ;
		for ( decltype( av_format_cam1->nb_streams ) i = 0 ; i < av_format_cam1->nb_streams ; ++i )
		{
			if ( av_format_cam1->streams[ i ]->codec->codec_type == AVMEDIA_TYPE_VIDEO )
			{
				decoder_cam1 = av_format_cam1->streams[ i ]->codec ;
				break ;
			}
		}

		if ( decoder_cam1.InvalidHandle() )
		{
			throw -1 ;
		}
		
		if ( avcodec_open2( decoder_cam1, avcodec_find_decoder( decoder_cam1->codec_id ), nullptr ) != 0 )
		{
			throw -1 ;
		}

		auto cam1_avctx_tb = decoder_cam1->time_base ;
		auto cam1_stream_tb = av_format_cam1->streams[ 0 ]->time_base ;

		Cw2FFmpegSws sws_cam1 ;
		if ( !sws_cam1.Init( decoder_cam1->pix_fmt, AV_PIX_FMT_YUV420P, decoder_cam1->coded_width, decoder_cam1->coded_height ) )
		{
			throw -1 ;
		}
		
		Cw2FFmpegPictureFrame frame_video_yuv_cam1 ;
		frame_video_yuv_cam1.Init( sws_cam1.m_dst.pix_fmt, sws_cam1.m_dst.width, sws_cam1.m_dst.height ) ;
		Cw2FFmpegAVFrame frame_video_cam1 = av_frame_alloc() ;
		
		CwRenderDirectDraw render_cam1 ;
		if ( !render_cam1.InitMediaRender( wnd_cam1, NULL ) )
		{
			throw -1 ;
		}
		
		CwSurfaceDirectDraw surface_cam1 ;
		if ( surface_cam1.InitSurface( sws_cam1.m_dst.width, sws_cam1.m_dst.height, 8,
			IMediaSurface::MSRS_TYPE_YV12, &render_cam1 ) != 0 )
		{
			throw -1 ;
		}

		//////////////////////////////////////////////////////////////////////////

		Cw2FFmpegAVFormatContext av_format_file = avformat_alloc_context() ;
		av_format_file->oformat = av_guess_format( nullptr, output, nullptr ) ;
		if ( av_format_file->oformat == nullptr )
		{
			throw -1 ;
		}

		auto video_stream = avformat_new_stream( av_format_file, 0 ) ;
		if ( video_stream == nullptr )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVCodecContextOpen encoder_video = video_stream->codec ;
		encoder_video->codec_id = AV_CODEC_ID_H264 ;
		encoder_video->codec_type = AVMEDIA_TYPE_VIDEO ;
		encoder_video->pix_fmt = sws_cam1.m_dst.pix_fmt ;
		encoder_video->width = sws_cam1.m_dst.width ;
		encoder_video->height = sws_cam1.m_dst.height ;
		encoder_video->time_base.den = 1 ;
		encoder_video->time_base.num = 30 ;
		encoder_video->bit_rate = 900000 ;
		encoder_video->gop_size = 12 ;
		encoder_video->me_range = 16 ;
		encoder_video->max_qdiff = 4 ;
		encoder_video->qcompress = 0.6f ;
		encoder_video->qmin = 10 ;
		encoder_video->qmax = 51 ;
		encoder_video->max_b_frames = 3 ;
		encoder_video->flags |= CODEC_FLAG_GLOBAL_HEADER ;

		video_stream->time_base.den = 30 ;
		video_stream->time_base.num = 1 ;
		
		Cw2FFmpegAVDictionary enc_options ;
		if ( encoder_video->codec_id == AV_CODEC_ID_H264 )
		{
			av_dict_set( enc_options, "preset", "ultrafast", 0 ) ;
			av_dict_set( enc_options, "tune", "zerolatency", 0 ) ;
		}

		if ( avcodec_open2( encoder_video, avcodec_find_encoder( encoder_video->codec_id ), enc_options ) != 0 )
		{
			throw -1 ;
		}

		Cw2FFmpegAVIOAuto io_file ;
		io_file.InitIO( av_format_file, output ) ;
		if ( !io_file.IsReady() )
		{
			throw -1 ;
		}

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

			//W2DBG( L"enc : %f, %d", tw.TickNow(), pts++ ) ;

			do
			{
				int got_frame = 0 ;
				auto ret = decode_video_packet( decoder_cam1, frame_video_cam1, pkt, got_frame ) ;
				if ( ret < 0 )
				{
					break ;
				}

				if ( got_frame )
				{

					if ( sws_cam1.Scale( frame_video_cam1->data, frame_video_cam1->linesize,
						frame_video_yuv_cam1.pic.data, frame_video_yuv_cam1.pic.linesize ) )
					{
						//RenderCam1( wnd_cam1, render_cam1, surface_cam1, frame_video_yuv_cam1.pic ) ;

						if ( true )
						{
							AVPacket enc_pkt ;
							av_init_packet( &enc_pkt ) ;
							enc_pkt.data = nullptr ;
							enc_pkt.size = 0 ;

							AVFrame enc_frame = { 0 } ;
							enc_frame.format = frame_video_yuv_cam1.ctx.pix_fmt ;
							enc_frame.width = frame_video_yuv_cam1.ctx.width ;
							enc_frame.height = frame_video_yuv_cam1.ctx.height ;
							enc_frame.data[ 0 ] = frame_video_yuv_cam1.pic.data[ 0 ] ;
							enc_frame.data[ 1 ] = frame_video_yuv_cam1.pic.data[ 1 ] ;
							enc_frame.data[ 2 ] = frame_video_yuv_cam1.pic.data[ 2 ] ;
							enc_frame.linesize[ 0 ] = frame_video_yuv_cam1.pic.linesize[ 0 ] ;
							enc_frame.linesize[ 1 ] = frame_video_yuv_cam1.pic.linesize[ 1 ] ;
							enc_frame.linesize[ 2 ] = frame_video_yuv_cam1.pic.linesize[ 2 ] ;
							
							//enc_frame.pts = tw.TickNow() / av_q2d( video_stream->time_base ) ;
							//enc_frame.pts = av_rescale_q( enc_frame.pts, encoder_video->time_base, video_stream->time_base ) ;
								
							//frame_video_cam1->pkt_pts ;
// 							av_rescale_q( pts++, encoder_video->time_base, video_stream->time_base ) ;
// 
// 							auto dd = frame_video_cam1->pkt_pts * av_q2d( cam1_stream_tb ) ;
// 							W2DBG( L"timestamp : %.2f", dd ) ;
// 
// 							av_rescale_q
							
							int enc_ok = 0 ;
							avcodec_encode_video2( encoder_video, &enc_pkt, &enc_frame, &enc_ok ) ;
							if ( enc_ok )
							{
								enc_pkt.stream_index = video_stream->index ;
								io_file.WritePacket( enc_pkt ) ;
							}
							
							av_free_packet( &enc_pkt ) ;
						}
					}
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
		
		int end = 0 ;
	}

	catch( ... )
	{
		
	}

	avformat_close_input( av_format_cam1 ) ;
	avformat_close_input( av_format_cam2 ) ;
	return 0 ;
	*/
}

//////////////////////////////////////////////////////////////////////////
