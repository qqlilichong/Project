// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

// TODO: 在此处引用程序需要的其他头文件

#include <WSL/WSL_Dialog.h>
#include <WSL/WSL_Render.h>

#define W2STL_ALL
#include <WSL/WSL2_Win.h>
#include <WSL/WSL2_STL.h>
#include <WSL/WSL2_Sync.h>
#include <WSL/WSL2_Memory.h>
#include <WSL/WSL2_General.h>
#include <WSL/WSL2_Thread.h>
#include <WSL/WSL2_IO.h>
#include <WSL/WSL2_D3D9.h>
#include <WSL/WSL2_DSound.h>
#include <WSL/WSL2_FFmpeg.h>
#include <WSL/WSL2_D3D9.h>

#define	D3DFMT_NV12		(D3DFORMAT)MAKEFOURCC( 'N', 'V', '1', '2' )
#define	D3DFMT_YV12		(D3DFORMAT)MAKEFOURCC( 'Y', 'V', '1', '2' )
