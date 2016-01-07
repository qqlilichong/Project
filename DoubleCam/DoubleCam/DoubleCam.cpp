// DoubleCam.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////

int RenderCam1( HWND wnd, CwRenderDirectDraw& render, CwSurfaceDirectDraw& surface, AVPicture& pic )
{
	LPVOID pBits = nullptr ;
	if ( surface.LockSurface( &pBits, 0 ) == 0 )
	{
		return -1 ;
	}
	
	AVPicture yuv = pic ;
	BITMAPINFOHEADER bih = surface.GetSurfaceContext() ;
	
	const DWORD w_uv = bih.biWidth / 2 ;
	const DWORD h_uv = bih.biHeight / 2 ;

	const DWORD line_dst_y = bih.biClrUsed ;
	const DWORD line_dst_uv = line_dst_y / 2 ;
	byte* dst_y = (byte*)pBits ;
	byte* dst_v = dst_y + ( line_dst_y * bih.biHeight ) ;
	byte* dst_u = dst_v + ( line_dst_uv * h_uv ) ;

	for ( DWORD i = 0 ; i < h_uv ; i++ )
	{
		memcpy( dst_y, yuv.data[ 0 ], bih.biWidth ) ;
		dst_y += line_dst_y ;
		yuv.data[ 0 ] += yuv.linesize[ 0 ] ;
		memcpy( dst_y, yuv.data[ 0 ], bih.biWidth ) ;
		dst_y += line_dst_y ;
		yuv.data[ 0 ] += yuv.linesize[ 0 ] ;

		memcpy( dst_v, yuv.data[ 2 ], w_uv ) ;
		dst_v += line_dst_uv ;
		yuv.data[ 2 ] += yuv.linesize[ 2 ] ;

		memcpy( dst_u, yuv.data[ 1 ], w_uv ) ;
		dst_u += line_dst_uv ;
		yuv.data[ 1 ] += yuv.linesize[ 1 ] ;
	}
	
	surface.UnlockSurface() ;
	
	RECT rc = { 0 } ;
	GetWindowRect( wnd, &rc ) ;
	render.RenderSurface( &surface, &rc, NULL, 0 ) ;
	return 0 ;
}

int decode_video_packet( AVCodecContext* ctx, AVFrame* frame, AVPacket& pkt, int& got_frame )
{
	int decoded = pkt.size ;
	
	auto ret = avcodec_decode_video2( ctx, frame, &got_frame, &pkt ) ;
	if ( ret < 0 )
	{
		return ret ;
	}
	
	return decoded ;
}

int _tmain( int argc, _TCHAR* argv[] )
{
	Cw2FFmpegAllSupport s1 ;
	Cw2FFmpegDeviceSupport s2 ;
	Cw2FFmpegAVCodecSupport s3 ;

	try
	{
		Cw2FFmpegAVFormatContext av_format_file = avformat_alloc_context() ;
		av_format_file->oformat = av_guess_format( nullptr, "test.mp4", nullptr ) ;
		if ( av_format_file->oformat == nullptr )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVIOContext io_ctx ;
		if ( avio_open( io_ctx, "test.mp4", AVIO_FLAG_READ_WRITE ) < 0 )
		{
			throw -1 ;
		}
		
		auto video_stream = avformat_new_stream( av_format_file, 0 ) ;
		if ( video_stream == nullptr )
		{
			throw -1 ;
		}
		
		Cw2FFmpegAVCodecContextOpen encoder_video ;
		encoder_video = video_stream->codec ;

		video_stream->time_base.num = 1 ;
		video_stream->time_base.den = 25 ;
		
		encoder_video->codec_id = AV_CODEC_ID_H264 ;
		encoder_video->codec_type = AVMEDIA_TYPE_VIDEO ;
		encoder_video->pix_fmt = PIX_FMT_YUV420P ;
		encoder_video->width = 640 ;
		encoder_video->height = 480 ;
		encoder_video->time_base = video_stream->time_base ;
		encoder_video->bit_rate = 400000 ;
		encoder_video->gop_size = 250 ;
		encoder_video->me_range = 16 ;
		encoder_video->max_qdiff = 4 ;
		encoder_video->qcompress = 0.6f ;
		encoder_video->qmin = 10 ;
		encoder_video->qmin = 51 ;
		encoder_video->max_b_frames = 3 ;
		
		Cw2FFmpegAVDictionary options ;
		if ( encoder_video->codec_id == AV_CODEC_ID_H264 )
		{
			av_dict_set( options, "preset", "ultrafast", 0 ) ;
			av_dict_set( options, "tune", "zerolatency", 0 ) ;
		}
		
		if ( avcodec_open2( encoder_video, avcodec_find_encoder( encoder_video->codec_id ), options ) != 0 )
		{
			throw -1 ;
		}

		int end = 0 ;
	}

	catch( ... )
	{

	}

	return 0 ;

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
		av_dict_set( options, "video_size", "160x120", 0 ) ;
		av_dict_set( options, "framerate", "15", 0 ) ;
		av_dict_set( options, "pixel_format", "yuyv422", 0 ) ;
		if ( avformat_open_input( av_format_cam1, "video=USB2.0 PC CAMERA", if_dshow_ptr, options ) != 0 )
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

		Cw2FFmpegSws sws_cam1 ;
		if ( !sws_cam1.Init( decoder_cam1->pix_fmt, AV_PIX_FMT_YUV420P, decoder_cam1->coded_width, decoder_cam1->coded_height ) )
		{
			throw -1 ;
		}
		
		Cw2FFmpegPictureFrame frame_video_yuv_cam1( sws_cam1.m_dst.pix_fmt, sws_cam1.m_dst.width, sws_cam1.m_dst.height ) ;
		Cw2FFmpegAVFrame frame_video_cam1 = av_frame_alloc() ;

		HWND wnd_cam1 = (HWND)0x003603F2 ;
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
						RenderCam1( wnd_cam1, render_cam1, surface_cam1, frame_video_yuv_cam1.pic ) ;
					}
				}

				pkt.data += ret ;
				pkt.size -= ret ;

			} while ( pkt.size > 0 ) ;
			
			av_free_packet( &orig_pkt ) ;
		}
		
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
