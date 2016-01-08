// DoubleCam.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

//////////////////////////////////////////////////////////////////////////

inline void DoubleCam_FFmpegInit()
{
	Cw2FFmpegAllSupport s1 ;
	Cw2FFmpegDeviceSupport s2 ;
	Cw2FFmpegAVCodecSupport s3 ;
}

__interface IW2_FFSINK
{
	virtual int OnPictureFrame( Cw2FFmpegPictureFrame& frame ) = 0 ;
};

class Cw2DrawWnd : public IW2_FFSINK
{
public:
	Cw2DrawWnd()
	{
		m_draw_wnd = NULL ;
	}

public:
	int Init( HWND hWnd )
	{
		if ( !m_render.InitMediaRender( hWnd, NULL ) )
		{
			return -1 ;
		}
		
		m_draw_wnd = hWnd ;
		return 0 ;
	}

private:
	virtual int OnPictureFrame( Cw2FFmpegPictureFrame& frame )
	{
		if ( frame.ctx.pix_fmt != AV_PIX_FMT_YUV420P )
		{
			return 0 ;
		}
		
		if ( m_surface.InitSurface( frame.ctx.width, frame.ctx.height, 8, IMediaSurface::MSRS_TYPE_YV12, &m_render ) == 0 )
		{
			LPVOID pBits = nullptr ;
			if ( m_surface.LockSurface( &pBits, 0 ) != 0 )
			{
				AVPicture yuv = frame.pic ;
				BITMAPINFOHEADER bih = m_surface.GetSurfaceContext() ;

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

				m_surface.UnlockSurface() ;

				RECT rc = { 0 } ;
				GetWindowRect( m_draw_wnd, &rc ) ;
				m_render.RenderSurface( &m_surface, &rc, NULL, 0 ) ;
			}
		}

		return 0 ;
	}

private:
	HWND				m_draw_wnd	;
	CwRenderDirectDraw	m_render	;
	CwSurfaceDirectDraw	m_surface	;
};

class Cw2OpenCam : public IThreadEngine2Routine
{
public:
	Cw2OpenCam() : m_engine( this )
	{
		m_frame_video_cam = av_frame_alloc() ;
	}

	~Cw2OpenCam()
	{
		CloseCam() ;
	}

public:
	int OpenCam( const char* cam_id,
		const char* _s = "352x288",
		const char* _r = "25",
		const char* _p = "yuyv422" )
	{
		if ( m_avfc_cam.InvalidHandle() && m_vdecoder_cam.InvalidHandle() )
		{
			if ( InitCam( cam_id, _s, _r, _p ) == 0 )
			{
				m_engine.EngineStart() ;
				return 0 ;
			}
		}
		
		return -1 ;
	}
	
	void CloseCam()
	{
		m_engine.EngineStop() ;
		
		m_yuv_frame.Uninit() ;
		m_yuv_sws.Uninit() ;

		m_frame_video_cam.FreeHandle() ;
		m_vdecoder_cam.FreeHandle() ;
		m_avfc_cam.FreeHandle() ;
	}

	void AddSink( IW2_FFSINK* pSink )
	{
		m_list_ffsink.push_back( pSink ) ;
	}

private:
	virtual int OnEngineRoutine( DWORD tid )
	{
		this->ReadPacket() ;
		return 0 ;
	}

private:
	int InitCam( const char* cam_id, const char* _s, const char* _r, const char* _p )
	{
		auto& vdecoder_cam = m_vdecoder_cam ;
		auto& avfc_cam = m_avfc_cam ;
		auto& yuv_sws = m_yuv_sws ;
		auto& yuv_frame = m_yuv_frame ;

		try
		{
			auto avif_dshow_ptr = av_find_input_format( "dshow" ) ;
			if ( avif_dshow_ptr == nullptr )
			{
				throw -1 ;
			}

			Cw2FFmpegAVDictionary avif_options ;

			if ( _s )
			{
				av_dict_set( avif_options, "video_size", _s, 0 ) ;
			}

			if ( _r )
			{
				av_dict_set( avif_options, "framerate", _r, 0 ) ;
			}

			if ( _p )
			{
				av_dict_set( avif_options, "pixel_format", _p, 0 ) ;
			}

			if ( avformat_open_input( avfc_cam,
				WSL2_String_FormatA( "video=%s", cam_id ).c_str(),
				avif_dshow_ptr, avif_options ) != 0 )
			{
				throw -1 ;
			}

			if ( avformat_find_stream_info( avfc_cam, nullptr ) < 0 )
			{
				throw -1 ;
			}

			for ( decltype( avfc_cam->nb_streams ) stream_idx = 0 ; stream_idx < avfc_cam->nb_streams ; ++stream_idx )
			{
				auto avcc = avfc_cam->streams[ stream_idx ]->codec ;
				if ( avcc->codec_type == AVMEDIA_TYPE_VIDEO )
				{
					if ( avcodec_open2( avcc, avcodec_find_decoder( avcc->codec_id ), nullptr ) != 0 )
					{
						throw -1 ;
					}

					avcc->opaque = (decltype(avcc->opaque))stream_idx ;
					vdecoder_cam = avcc ;
					break ;
				}
			}
			
			if ( !yuv_sws.Init( vdecoder_cam->pix_fmt, AV_PIX_FMT_YUV420P,
				vdecoder_cam->coded_width, vdecoder_cam->coded_height ) )
			{
				throw -1 ;
			}
			
			if ( yuv_frame.Init( yuv_sws.m_dst.pix_fmt, yuv_sws.m_dst.width, yuv_sws.m_dst.height ) != 0 )
			{
				throw -1 ;
			}
		}

		catch( ... )
		{
			yuv_frame.Uninit() ;
			yuv_sws.Uninit() ;
			vdecoder_cam.FreeHandle() ;
			avfc_cam.FreeHandle() ;
			return -1 ;
		}
		
		return 0 ;
	}

	int ReadPacket()
	{
		auto& avfc_cam = m_avfc_cam ;
		auto& vdecoder_cam = m_vdecoder_cam ;
		auto& frame_video_cam = m_frame_video_cam ;

		if ( true )
		{
			AVPacket pkt ;
			av_init_packet( &pkt ) ;
			pkt.data = nullptr ;
			pkt.size = 0 ;
			if ( av_read_frame( avfc_cam, &pkt ) >= 0 )
			{
				auto orig_pkt = pkt ;

				do
				{
					auto ret = this->DecodePacket( pkt ) ;
					if ( ret < 0 )
					{
						break ;
					}

					pkt.data += ret ;
					pkt.size -= ret ;

				} while( pkt.size > 0 ) ;

				av_free_packet( &orig_pkt ) ;
			}
		}

		return 0 ;
	}
	
	int DecodePacket( AVPacket& pkt )
	{
		auto& vdecoder_cam = m_vdecoder_cam ;
		auto& frame_video_cam = m_frame_video_cam ;
		auto& yuv_sws = m_yuv_sws ;
		auto& yuv_frame = m_yuv_frame ;
		
		int decoded = pkt.size ;
		auto video_stream_index = (int)vdecoder_cam->opaque ;

		if ( video_stream_index == pkt.stream_index )
		{
			int got_frame = 0 ;
			auto ret = avcodec_decode_video2( vdecoder_cam, frame_video_cam, &got_frame, &pkt ) ;
			if ( ret < 0 )
			{
				return ret ;
			}

			if ( got_frame )
			{
				if ( yuv_sws.Scale( frame_video_cam->data,
					frame_video_cam->linesize,
					yuv_frame.pic.data,
					yuv_frame.pic.linesize ) )
				{
					for ( auto& sink : m_list_ffsink )
					{
						sink->OnPictureFrame( yuv_frame ) ;
					}
				}
			}
		}

		return decoded ;
	}

private:
	Cw2FFmpegAVFormatContext	m_avfc_cam			;
	Cw2FFmpegAVCodecContextOpen	m_vdecoder_cam		;
	Cw2FFmpegAVFrame			m_frame_video_cam	;
	
	Cw2FFmpegSws				m_yuv_sws			;
	Cw2FFmpegPictureFrame		m_yuv_frame			;

	list< IW2_FFSINK* >			m_list_ffsink		;

private:
	Cw2ThreadEngine2			m_engine			;
};

int _tmain( int argc, _TCHAR* argv[] )
{
	DoubleCam_FFmpegInit() ;

	Cw2DrawWnd dw1 ;
	if ( dw1.Init( (HWND)0x00020552 ) != 0 )
	{
		return -1 ;
	}

	Cw2DrawWnd dw2 ;
	if ( dw2.Init( (HWND)0x006703EC ) != 0 )
	{
		return -1 ;
	}
	
	Cw2OpenCam cam1 ;
	cam1.AddSink( &dw1 ) ;
	if ( cam1.OpenCam( "USB2.0 PC CAMERA", nullptr, nullptr, nullptr ) != 0 )
	{
		return -1 ;
	}

	Cw2OpenCam cam2 ;
	cam2.AddSink( &dw2 ) ;
	if ( cam2.OpenCam( "Vega USB 2.0 Camera.", nullptr, nullptr, nullptr ) != 0 )
	{
		return -1 ;
	}

	Sleep( 1000 * 20 ) ;

	cam2.CloseCam() ;
	cam1.CloseCam() ;
	
	return 0 ;

	/*

	HWND wnd_cam1 = (HWND)0x00020552 ;
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
		av_dict_set( options, "video_size", "352x288", 0 ) ;
		av_dict_set( options, "framerate", "25", 0 ) ;
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

		auto timebase_cam1 = decoder_cam1->time_base ;

		Cw2FFmpegSws sws_cam1 ;
		if ( !sws_cam1.Init( decoder_cam1->pix_fmt, AV_PIX_FMT_YUV420P, decoder_cam1->coded_width, decoder_cam1->coded_height ) )
		{
			throw -1 ;
		}
		
		Cw2FFmpegPictureFrame frame_video_yuv_cam1( sws_cam1.m_dst.pix_fmt, sws_cam1.m_dst.width, sws_cam1.m_dst.height ) ;
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
		encoder_video->time_base = video_stream->time_base = timebase_cam1 ;
		encoder_video->bit_rate = 900000 ;
		encoder_video->gop_size = 12 ;
		encoder_video->me_range = 16 ;
		encoder_video->max_qdiff = 4 ;
		encoder_video->qcompress = 0.6f ;
		encoder_video->qmin = 10 ;
		encoder_video->qmax = 51 ;
		encoder_video->max_b_frames = 3 ;
		encoder_video->flags |= CODEC_FLAG_GLOBAL_HEADER ;
		
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

		Cw2FFmpegAVIOAuto io_file( av_format_file, output ) ;
		if ( !io_file.IsReady() )
		{
			throw -1 ;
		}

		//////////////////////////////////////////////////////////////////////////

		Cw2TickCount tw ;
		
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

						if ( true )
						{
							static int64_t pts = 0 ;

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

							enc_frame.pts = av_rescale_q( pts++, encoder_video->time_base, video_stream->time_base ) ;

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
