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
	virtual int OnAVFrame( Cw2FFmpegAVFrame& frame ) = 0 ;
	virtual int OnRenderUpdate( IDirect3DDevice9* pRender, LPRECT pDst, LPRECT pSrc ) = 0 ;
	virtual int OnSurfaceReady( IDirect3DSurface9* pSurface ) = 0 ;
};

class Cw2FFSink : public IW2_FFSINK
{
public:
	virtual int OnAVFrame( Cw2FFmpegAVFrame& frame )
	{
		return -1 ;
	}

	virtual int OnRenderUpdate( IDirect3DDevice9* pRender, LPRECT pDst, LPRECT pSrc )
	{
		return -1 ;
	}

	virtual int OnSurfaceReady( IDirect3DSurface9* pSurface )
	{
		return -1 ;
	}
};

class Cw2FFSinkList
{
public:
	void AddSink( IW2_FFSINK* pSink )
	{
		if ( pSink )
		{
			m_list.push_back( pSink ) ;
		}
	}

	void Invoke_OnAVFrame( Cw2FFmpegAVFrame& frame )
	{
		for ( auto& sink : m_list )
		{
			sink->OnAVFrame( frame ) ;
		}
	}

	void Invoke_OnRenderUpdate( IDirect3DDevice9* pRender, LPRECT pDst, LPRECT pSrc )
	{
		for ( auto& sink : m_list )
		{
			sink->OnRenderUpdate( pRender, pDst, pSrc ) ;
		}
	}

	void Invoke_OnSurfaceReady( IDirect3DSurface9* pSurface )
	{
		for ( auto& sink : m_list )
		{
			sink->OnSurfaceReady( pSurface ) ;
		}
	}

private:
	list< IW2_FFSINK* >	m_list ;
};

//////////////////////////////////////////////////////////////////////////

class Cw2FileEncoder : public Cw2FFSink
{
public:
	Cw2FileEncoder()
	{
		m_video_pts = 0 ;
	}

	~Cw2FileEncoder()
	{
		Uninit() ;
	}

public:
	int Init( int width, int height, int framerate, const char* url )
	{
		if ( !m_avfc_file.InvalidHandle() )
		{
			return -1 ;
		}

		Cw2FFmpegAVFormatContext& avfc_file = m_avfc_file ;
		Cw2FFmpegAVCodecContextOpen& encoder_video = m_encoder_video ;
		Cw2FFmpegAVIOAuto& io_file = m_io_file ;

		try
		{
			avfc_file = avformat_alloc_context() ;
			avfc_file->oformat = av_guess_format( nullptr, url, nullptr ) ;
			if ( avfc_file->oformat == nullptr )
			{
				throw -1 ;
			}

			auto stream_video = avformat_new_stream( avfc_file, 0 ) ;
			if ( stream_video == nullptr )
			{
				throw -1 ;
			}
			
			encoder_video = stream_video->codec ;
			encoder_video->codec_id = AV_CODEC_ID_H264 ;
			encoder_video->codec_type = AVMEDIA_TYPE_VIDEO ;
			encoder_video->pix_fmt = AV_PIX_FMT_YUV420P ;
			encoder_video->width = width ;
			encoder_video->height = height ;
			encoder_video->time_base = stream_video->time_base = AVRational{ 1, framerate } ;
			encoder_video->bit_rate = 400000 ;
			encoder_video->gop_size = 250 ;
			encoder_video->me_range = 16 ;
			encoder_video->max_qdiff = 4 ;
			encoder_video->qcompress = 0.6f ;
			encoder_video->qmin = 10 ;
			encoder_video->qmax = 51 ;
			encoder_video->max_b_frames = 3 ;
			encoder_video->flags |= CODEC_FLAG_GLOBAL_HEADER ;
			encoder_video->opaque = stream_video ;

			Cw2FFmpegAVDictionary encoder_options ;
			if ( encoder_video->codec_id == AV_CODEC_ID_H264 )
			{
				av_dict_set( encoder_options, "preset", "ultrafast", 0 ) ;
				av_dict_set( encoder_options, "tune", "zerolatency", 0 ) ;
			}
		
			if ( avcodec_open2( encoder_video, avcodec_find_encoder( encoder_video->codec_id ), encoder_options ) != 0 )
			{
				throw -1 ;
			}
			
			if ( io_file.InitIO( avfc_file, url ) != 0 )
			{
				throw -1 ;
			}
		}

		catch ( ... )
		{
			Uninit() ;
			return -1 ;
		}

		return 0 ;
	}

	void Uninit()
	{
		m_io_file.FinishIO() ;

		m_encoder_video.FreeHandle() ;

		m_avfc_file.FreeHandle() ;
	}

private:
	virtual int OnSurfaceReady( IDirect3DSurface9* pSurface )
	{
		if ( m_avfc_file.InvalidHandle() )
		{
			return 0 ;
		}
		
		Cw2FFmpegAVCodecContextOpen& encoder_video = m_encoder_video ;
		auto video_stream = (AVStream*)encoder_video->opaque ;
		Cw2FFmpegAVIOAuto& io_file = m_io_file ;
		
		D3DSURFACE_DESC desc ;
		if ( pSurface->GetDesc( &desc ) != 0 )
		{
			return 0 ;
		}

		if ( desc.Width != encoder_video->width )
		{
			return 0 ;
		}
		
		if ( desc.Height != encoder_video->height )
		{
			return 0 ;
		}

		AVPixelFormat pix_fmt = AV_PIX_FMT_NONE ;
		switch ( desc.Format )
		{
		case D3DFMT_X8R8G8B8 :
			pix_fmt = AV_PIX_FMT_BGRA ;
			break ;

		default :
			return 0 ;
		}
		
		Cw2FFmpegSws sws ;
		if ( !sws.Init( pix_fmt, m_encoder_video->pix_fmt, encoder_video->width, encoder_video->height ) )
		{
			return 0 ;
		}

		Cw2FFmpegPictureFrame encode_frame ;
		if ( encode_frame.Init( sws.m_dst.pix_fmt, sws.m_dst.width, sws.m_dst.height ) != 0 )
		{
			return 0 ;
		}

		bool sws_ok = false ;
		D3DLOCKED_RECT d3dlr = { 0 } ;
		if ( pSurface->LockRect( &d3dlr, NULL, D3DLOCK_DONOTWAIT ) == 0 )
		{
			AVPicture src_pic = { 0 } ;
			src_pic.data[ 0 ] = (byte*)d3dlr.pBits ;
			src_pic.linesize[ 0 ] = d3dlr.Pitch ;

			if ( sws.Scale( src_pic.data, src_pic.linesize, encode_frame.pic.data, encode_frame.pic.linesize ) )
			{
				sws_ok = true ;
			}
					
			pSurface->UnlockRect() ;
		}

		if ( !sws_ok )
		{
			return 0 ;
		}
		
		AVPacket enc_pkt ;
		av_init_packet( &enc_pkt ) ;
		enc_pkt.data = nullptr ;
		enc_pkt.size = 0 ;
		
		AVFrame enc_frame = { 0 } ;
		enc_frame.format = encode_frame.ctx.pix_fmt ;
		enc_frame.width = encode_frame.ctx.width ;
		enc_frame.height = encode_frame.ctx.height ;
		enc_frame.data[ 0 ] = encode_frame.pic.data[ 0 ] ;
		enc_frame.data[ 1 ] = encode_frame.pic.data[ 1 ] ;
		enc_frame.data[ 2 ] = encode_frame.pic.data[ 2 ] ;
		enc_frame.linesize[ 0 ] = encode_frame.pic.linesize[ 0 ] ;
		enc_frame.linesize[ 1 ] = encode_frame.pic.linesize[ 1 ] ;
		enc_frame.linesize[ 2 ] = encode_frame.pic.linesize[ 2 ] ;
		
		enc_frame.pts = av_rescale_q( m_video_pts++, encoder_video->time_base, video_stream->time_base ) ;
		
		int enc_ok = 0 ;
		avcodec_encode_video2( encoder_video, &enc_pkt, &enc_frame, &enc_ok ) ;
		if ( enc_ok )
		{
			enc_pkt.stream_index = video_stream->index ;
			io_file.WritePacket( enc_pkt ) ;
		}
		
		av_free_packet( &enc_pkt ) ;
		
		return 0 ;
	}

private:
	Cw2FFmpegAVFormatContext	m_avfc_file		;
	
	Cw2FFmpegAVCodecContextOpen m_encoder_video	;
	int64_t						m_video_pts		;

	Cw2FFmpegAVIOAuto			m_io_file		;
};

class Cw2DrawPad : public Cw2FFSink, public Cw2FFSinkList, public IW2_IOCP_STATUS
{
public:
	Cw2DrawPad() : m_vpp( this )
	{
		m_vpp.EnterVPP() ;
	}

public:
	int Init( int width, int height, vector< RECT > vecView )
	{
		for ( auto& rc : vecView )
		{
			RCSTATUS it ;
			it.rcView = rc ;
			it.nStatus = 0 ;
			m_view_map.push_back( it ) ;
		}
		
		return 0 ;
	}

private:
	virtual int OnRenderUpdate( IDirect3DDevice9* pRender, LPRECT pDst, LPRECT pSrc )
	{
		//if ( UpdateViewMap( pDst ) )
		{
			m_vpp.PostVPP( 0, AVMEDIA_TYPE_VIDEO, pRender ) ;
		}
		
		return 0 ;
	}
	
	bool UpdateViewMap( LPRECT pRC )
	{
		Cw2AutoLock< decltype( m_lock ) > lock( &m_lock ) ;

		if ( pRC == nullptr )
		{
			return false ;
		}

		RECT& rc = *pRC ;

		for ( auto& view : m_view_map )
		{
			if ( view.rcView.left != rc.left )
			{
				continue ;
			}

			if ( view.rcView.top != rc.top )
			{
				continue ;
			}

			if ( view.rcView.right != rc.right )
			{
				continue ;
			}

			if ( view.rcView.bottom != rc.bottom )
			{
				continue ;
			}

			view.nStatus = 1 ;
			return IsViewMapReady() ;
		}
		
		return false ;
	}

	bool IsViewMapReady()
	{
		for ( auto& view : m_view_map )
		{
			if ( view.nStatus == 0 )
			{
				return false ;
			}
		}

		ResetViewMap() ;
		return true ;
	}

	void ResetViewMap()
	{
		for ( auto& view : m_view_map )
		{
			view.nStatus = 0 ;
		}
	}

	struct RCSTATUS
	{
		RECT	rcView	;
		int		nStatus	;
	};

private:
	virtual int OnGetStatus( LONGLONG iocp_length, ULONG_PTR iocp_id,
		DWORD NumberOfBytesTransferred,
		ULONG_PTR CompletionKey,
		LPVOID lpOverlapped )
	{
		if ( NumberOfBytesTransferred == 0 && CompletionKey == 0 && lpOverlapped == nullptr )
		{
			return -1 ;
		}

		if ( CompletionKey == AVMEDIA_TYPE_VIDEO )
		{
			OnVideoPadReady( (IDirect3DDevice9*)lpOverlapped ) ;
		}

		return 0 ;
	}
	
	virtual int OnCleanStatus( ULONG_PTR iocp_id,
		DWORD NumberOfBytesTransferred,
		ULONG_PTR CompletionKey,
		LPVOID lpOverlapped )
	{
		return 0 ;
	}

private:
	int OnVideoPadReady( IDirect3DDevice9* pRender )
	{
		CComQIPtr< IDirect3DSurface9 > backsurface ;
		if ( pRender->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &backsurface ) != 0 )
		{
			return 0 ;
		}

		D3DSURFACE_DESC desc ;
		if ( backsurface->GetDesc( &desc ) != 0 )
		{
			return 0 ;
		}
			
		CComQIPtr< IDirect3DSurface9 > copysurface ;
		if ( pRender->CreateOffscreenPlainSurface( desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copysurface, NULL ) != 0 )
		{
			return 0 ;
		}

		if ( pRender->GetRenderTargetData( backsurface, copysurface ) != 0 )
		{
			return 0 ;
		}
		
		this->Invoke_OnSurfaceReady( copysurface ) ;
		return 0 ;
	}

private:
	Cw2US_CS			m_lock		;
	vector< RCSTATUS >	m_view_map	;
	Cw2VPP				m_vpp		;
};

class Cw2DrawWnd : public Cw2FFSink, public Cw2FFSinkList
{
public:
	Cw2DrawWnd()
	{
		RECT rc = { 0 } ;
		m_rc = rc ;
		m_draw_wnd = NULL ;
		m_d3dev_ptr = nullptr ;
	}

public:
	int Init( HWND hWnd, RECT rc, Cw2D3D9Device* ptr )
	{
		if ( ptr == nullptr || m_d3dev_ptr != nullptr )
		{
			return -1 ;
		}
		
		m_d3dev_ptr = ptr ;
		m_draw_wnd = hWnd ;
		m_rc = rc ;
		return 0 ;
	}

private:
	virtual int OnAVFrame( Cw2FFmpegAVFrame& frame )
	{
		AVPixelFormat fmt = (AVPixelFormat)frame->format ;
		const int width = frame->width ;
		const int height = frame->height ;

		D3DFORMAT d3dfmt = D3DFMT_UNKNOWN ;
		switch ( frame->format )
		{
		case AV_PIX_FMT_YUYV422 :
			d3dfmt = D3DFMT_YUY2 ;
			break ;
		
		default :
			return 0 ;
		}
		
		CComQIPtr< IDirect3DSurface9 > surface ;
		if ( m_d3dev_ptr->CreateOffscreenSurface( width, height, d3dfmt, &surface ) != 0 )
		{
			return 0 ;
		}

		switch ( d3dfmt )
		{
			case D3DFMT_YUY2 :
			{
				D3DLOCKED_RECT d3dlr = { 0 } ;
				if ( surface->LockRect( &d3dlr, NULL, D3DLOCK_DONOTWAIT ) == 0 )
				{
					AVPicture dst_pic = { 0 } ;
					dst_pic.data[ 0 ] = (byte*)d3dlr.pBits ;
					dst_pic.linesize[ 0 ] = d3dlr.Pitch ;

					av_image_copy( dst_pic.data,
						dst_pic.linesize,
						(const uint8_t**)frame->data,
						frame->linesize,
						fmt, width, height ) ;
					
					surface->UnlockRect() ;
				}
			}
			
			break ;
		}
		
		if ( m_d3dev_ptr->UpdateBackSurface( surface, &m_rc ) == 0 )
		{
			if ( m_d3dev_ptr->Present( NULL, &m_rc, m_draw_wnd ) == 0 )
			{
				this->Invoke_OnRenderUpdate( *m_d3dev_ptr, &m_rc, NULL ) ;
			}
		}
		
		return 0 ;
	}

private:
	HWND			m_draw_wnd	;
	RECT			m_rc		;
	Cw2D3D9Device*	m_d3dev_ptr	;
};

class Cw2OpenCam : public IThreadEngine2Routine, public Cw2FFSinkList
{
public:
	Cw2OpenCam() : m_engine( this )
	{

	}

	~Cw2OpenCam()
	{
		CloseCam() ;
	}

public:
	int OpenCam( const char* cam_id,
		const int width,
		const int height,
		const int framerate,
		const char* _p = "yuyv422" )
	{
		if ( m_avfc_cam.InvalidHandle() && m_vdecoder_cam.InvalidHandle() )
		{
			auto _s = WSL2_String_FormatA( "%dx%d", width, height ) ;
			auto _r = WSL2_String_FormatA( "%d", framerate ) ;
			if ( OpenCam( cam_id, _s.c_str(), _r.c_str(), _p ) == 0 )
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
		
		m_frame_video_cam.FreeHandle() ;
		m_vdecoder_cam.FreeHandle() ;
		m_avfc_cam.FreeHandle() ;
	}

private:
	virtual int OnEngineRoutine( DWORD tid )
	{
		this->ReadPacket() ;
		return 0 ;
	}

private:
	int OpenCam( const char* cam_id, const char* _s, const char* _r, const char* _p )
	{
		auto& vdecoder_cam = m_vdecoder_cam ;
		auto& avfc_cam = m_avfc_cam ;

		try
		{
			auto avif_dshow_ptr = av_find_input_format( "dshow" ) ;
			if ( avif_dshow_ptr == nullptr )
			{
				throw -1 ;
			}

			Cw2FFmpegAVDictionary avif_options ;

// 			if ( _s )
// 			{
// 				av_dict_set( avif_options, "video_size", _s, 0 ) ;
// 			}
// 
// 			if ( _r )
// 			{
// 				av_dict_set( avif_options, "framerate", _r, 0 ) ;
// 			}
// 
// 			if ( _p )
// 			{
// 				av_dict_set( avif_options, "pixel_format", _p, 0 ) ;
// 			}

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
					
					avcc->opaque = (void*)stream_idx ;
					vdecoder_cam = avcc ;
					break ;
				}
			}
			
			m_frame_video_cam = av_frame_alloc() ;
			if ( m_frame_video_cam.InvalidHandle() )
			{
				throw -1 ;
			}
		}

		catch( ... )
		{
			m_frame_video_cam.FreeHandle() ;
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
				this->Invoke_OnAVFrame( frame_video_cam ) ;
			}
		}

		return decoded ;
	}

private:
	Cw2FFmpegAVFormatContext	m_avfc_cam			;
	Cw2FFmpegAVCodecContextOpen	m_vdecoder_cam		;
	Cw2FFmpegAVFrame			m_frame_video_cam	;

private:
	Cw2ThreadEngine2			m_engine			;
};

int decode_video_packet( Cw2FFmpegAVCodecContextOpen& vdecoder_cam, Cw2FFmpegAVFrame& frame_video_cam, AVPacket& pkt, int& got_frame )
{

		
	int decoded = pkt.size ;
	auto video_stream_index = (int)vdecoder_cam->opaque ;

	if ( video_stream_index == pkt.stream_index )
	{
		auto ret = avcodec_decode_video2( vdecoder_cam, frame_video_cam, &got_frame, &pkt ) ;
		if ( ret < 0 )
		{
			return ret ;
		}

		if ( got_frame )
		{

		}
	}

	return decoded ;
}

int _tmain( int argc, _TCHAR* argv[] )
{
	DoubleCam_FFmpegInit() ;
	
// 	const char* output_file = "test.mp4" ;
// 	const int video_width = 640 ;
// 	const int video_height = 480 ;
// 	const int video_framerate = 30 ;
// 	
// 	const int pad_width = video_width ;
// 	const int pad_height = video_height * 2 ;
// 	
// 	RECT rcViewA = { 0, 0, pad_width, pad_height / 2 } ;
// 	RECT rcViewB = { 0, pad_height / 2, pad_width, pad_height } ;
// 	vector< RECT > vecView ;
// 	vecView.push_back( rcViewA ) ;
// 	vecView.push_back( rcViewB ) ;
// 	
// 	Cw2D3D9 d3d9 ;
// 	Cw2D3D9Device d3dev ;
// 	if ( d3dev.CreateDevice( d3d9, GetDesktopWindow(), D3DADAPTER_DEFAULT,
// 		nullptr, pad_width, pad_height ) != 0 )
// 	{
// 		return 0 ;
// 	}
// 	
// 	Cw2FileEncoder file_encoder ;
// 	if ( file_encoder.Init( pad_width, pad_height, video_framerate, output_file ) != 0 )
// 	{
// 		return -1 ;
// 	}
// 	
// 	Cw2DrawPad pad ;
// 	pad.AddSink( &file_encoder ) ;
// 	if ( pad.Init( pad_width, pad_height, vecView ) != 0 )
// 	{
// 		return -1 ;
// 	}
// 	
// 	Cw2DrawWnd dw1 ;
// 	dw1.AddSink( &pad ) ;
// 	if ( dw1.Init( (HWND)0x000602C6, vecView[ 0 ], &d3dev ) != 0 )
// 	{
// 		return -1 ;
// 	}
// 
// 	Cw2DrawWnd dw2 ;
// 	dw2.AddSink( &pad ) ;
// 	if ( dw2.Init( (HWND)0x000703F2, vecView[ 1 ], &d3dev ) != 0 )
// 	{
// 		return -1 ;
// 	}
// 	
// 	Cw2OpenCam cam1 ;
// 	cam1.AddSink( &dw1 ) ;
// 	if ( cam1.OpenCam( "USB2.0 PC CAMERA", video_width, video_height, video_framerate ) != 0 )
// 	{
// 		return -1 ;
// 	}
// 	
// 	Cw2OpenCam cam2 ;
// 	cam2.AddSink( &dw2 ) ;
// 	if ( cam2.OpenCam( "Vega USB 2.0 Camera.", video_width, video_height, video_framerate ) != 0 )
// 	{
// 		return -1 ;
// 	}
// 	
// 	for ( ;; )
// 	{
// 		Sleep( 1 ) ;
// 	}
// 	
// 	Sleep( 1000 * 20 ) ;
// 	
// 	cam2.CloseCam() ;
// 	cam1.CloseCam() ;
// 	
// 	return 0 ;

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
		av_dict_set( options, "video_size", "352x288", 0 ) ;
		av_dict_set( options, "framerate", "30", 0 ) ;
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

		auto timebase_cam1 = decoder_cam1->time_base ;

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

			W2DBG( L"enc : %f, %d", tw.TickNow(), pts++ ) ;

			do
			{
				break ;

				int got_frame = 0 ;
				auto ret = decode_video_packet( decoder_cam1, frame_video_cam1, pkt, got_frame ) ;
				if ( ret < 0 )
				{
					break ;
				}

				if ( got_frame )
				{

// 					if ( sws_cam1.Scale( frame_video_cam1->data, frame_video_cam1->linesize,
// 						frame_video_yuv_cam1.pic.data, frame_video_yuv_cam1.pic.linesize ) )
					{
						//RenderCam1( wnd_cam1, render_cam1, surface_cam1, frame_video_yuv_cam1.pic ) ;

// 						if ( false )
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
// 
// 							enc_frame.pts = av_rescale_q( pts++, encoder_video->time_base, video_stream->time_base ) ;
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
}

//////////////////////////////////////////////////////////////////////////
