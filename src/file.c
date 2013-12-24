﻿/***********************************************************************************
 * Copyright (c) 2013, baickl(baickl@gmail.com)
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright notice, this
 * list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 * 
 * * Neither the name of the {organization} nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***********************************************************************************/
#include <vfs/file.h>
#include <vfs/vfs.h>
#include <vfs/util.h>
#include "vfs_private.h"
#include "pool.h"
#include <stdio.h>
#include <string.h>

/************************************************************************/
/* VFS文件结构                                                  */
/************************************************************************/
struct vfs_file_t
{
    VFS_UINT64		_M_size;
    VFS_UINT64		_M_position;
    VFS_VOID*		_M_buffer;
};

static FILE* sfopen(const VFS_CHAR* filename,const VFS_CHAR* mode)
{
#ifdef __linux__
	return fopen(filename,mode);
#else
	FILE* fp = NULL;
	VFS_INT32 err;

	err = fopen_s(&fp,filename,mode);
	if( err == 0 )
	{
		return fp;
	}
	else
	{
		return NULL;
	}
#endif
}

VFS_INT32 vfs_file_exists( const VFS_CHAR* file  )
{
    int i,count;
    vfs_archive_obj* _archive;
    VFS_UINT64 size;

    const VFS_CHAR*filefullpath;
    VFS_CHAR filepath[VFS_MAX_FILENAME+1];

    FILE* fp;

    /************************************************************************/

    if( !file)
        return VFS_FILE_NOT_EXISTS;

    /* 先尝试在包里查找 */
    count = vfs_get_archive_count();
    for( i = 0; i<count; ++i )
    {
        _archive = vfs_get_archive_index(i);
        if( _archive && VFS_TRUE == _archive->plugin->plugin.archive.archive_locate_item(_archive->archive,file,&size) )
            return VFS_FILE_EXISTS_IN_ARCHIVE;
    }


    /* 判断是否在本地 */
    memset(filepath,0,sizeof(filepath));
    filefullpath = file;
    if( vfs_util_path_combine(filepath,g_vfs->_M_workpath,file) )
    {
        filefullpath = filepath;
    }

    fp = sfopen(filefullpath,"rb");
    if( fp )
    {
        VFS_SAFE_FCLOSE(fp);
        return VFS_FILE_EXISTS_IN_DIR;
    }

    /* 不存在文件 */
    return VFS_FILE_NOT_EXISTS;
}

vfs_file* vfs_file_create(VFS_VOID *buf,VFS_UINT64 size)
{
	vfs_file* vff;

    if( !g_vfs )
        return NULL;

	vff = (vfs_file*)vfs_pool_malloc(sizeof(vfs_file));
	if( !vff )
	{
		return NULL;
	}

	if( !size)
	{
		vff->_M_buffer = 0;
		vff->_M_size = 0;
		vff->_M_position = 0;
	}
	else
	{
		vff->_M_buffer = (VFS_VOID*)vfs_pool_malloc((VFS_SIZE)size);
		if( !vff->_M_buffer )
		{
            vfs_pool_free(vff);
			return NULL;
		}
		vff->_M_position = 0;
		vff->_M_size = size;

		if( buf )
			memcpy(vff->_M_buffer,buf,(VFS_SIZE)size);
	}
	
	return vff;
}

vfs_file* vfs_file_open(const VFS_CHAR* file )
{
	VFS_INT32 i ;
	VFS_INT64 size;
	VFS_VOID* buf;
    vfs_archive_obj* p;
	vfs_file* vff;

	FILE* fp;

    const VFS_CHAR*filefullpath;
    VFS_CHAR filepath[VFS_MAX_FILENAME+1];

    if( !g_vfs )
        return NULL;

	if( !file )
		return NULL;

	/* 先尝试从包里读取 */
    for( i = 0; i<g_vfs->_M_count; ++i )
    {
        p = g_vfs->_M_archives[i];
        if( VFS_TRUE != p->plugin->plugin.archive.archive_locate_item(p->archive,file,&size) )
            continue;

        buf = (VFS_VOID*)vfs_pool_malloc((VFS_SIZE)size);
        if( !buf )
            return NULL;

        if( VFS_TRUE != p->plugin->plugin.archive.archive_unpack_item_by_filename(p->archive,file,buf,size) ) 
        {
            vfs_pool_free(buf);
            return NULL;
        }

        vff = vfs_file_create(0,0);
        if( !vff )
        {
            if(buf)
            {
                vfs_pool_free(buf);
                buf = NULL;
            }
            return NULL;
        }
        vff->_M_buffer = buf;
        vff->_M_size = size;
        vff->_M_position = 0;

        return vff;
    }


	/* 包里读取不出来，在本地读取 */
    memset(filepath,0,sizeof(filepath));
    filefullpath = file;
    if( vfs_util_path_combine(filepath,g_vfs->_M_workpath,file) )
    {
        filefullpath = filepath;
    }


	fp = sfopen(filefullpath,"rb");
	if( fp )
	{
		VFS_FSEEK(fp,0,SEEK_END);
		size = VFS_FTELL(fp);
		VFS_FSEEK(fp,0,SEEK_SET);
		buf = NULL;

		if( size > 0 )
		{
			buf = (VFS_VOID*)vfs_pool_malloc((VFS_SIZE)size);
			if( !buf )
			{
				VFS_SAFE_FCLOSE(fp);
				return NULL;
			}

			if( !VFS_CHECK_FREAD(fp,buf,size) )
			{
				VFS_SAFE_FCLOSE(fp);
				if(buf)
                {
                    vfs_pool_free(buf);
                    buf = NULL;
                }
				return NULL;
			}

			VFS_SAFE_FCLOSE(fp);
		}

		vff = vfs_file_create(0,0);
		if( !vff )
		{
			if(buf){
                vfs_pool_free(buf);
                buf = NULL;
            }
			return NULL;
		}
		vff->_M_buffer = buf;
		vff->_M_size = size;
		vff->_M_position = 0;

		return vff;
	}

	return NULL;
}

VFS_VOID vfs_file_close(vfs_file* file)
{
	if( file )
	{
		if(file->_M_buffer)
        {
            vfs_pool_free(file->_M_buffer);
            file->_M_buffer = NULL;
        }
		vfs_pool_free(file);
        file = NULL;
	}
}


VFS_BOOL vfs_file_eof(vfs_file* file )
{
	if( file && file->_M_position >= file->_M_size )
		return VFS_TRUE;
	else
		return VFS_FALSE;
}

VFS_UINT64 vfs_file_tell(vfs_file* file)
{
	if( file )
		return file->_M_position;

	return 0;
}

VFS_UINT64 vfs_file_size( vfs_file* file )
{
	if( !file )
		return 0;

	return file->_M_size;
}

const VFS_VOID* vfs_file_data( vfs_file* file )
{
    if( !file )
        return 0;

    return file->_M_buffer;
}

VFS_UINT64 vfs_file_seek(vfs_file* file,VFS_INT64 pos, VFS_INT32 mod )
{
	VFS_UINT64 _pos;
	if( !file )
		return -1;

	if( mod == SEEK_CUR )
	{
		_pos = file->_M_position + pos;
		if( _pos >= 0 && _pos < file->_M_size  )
			file->_M_position = pos;
	}
	else if( mod == SEEK_END )
	{
		_pos = file->_M_size -1 + pos;
		if( _pos >= 0 && _pos < file->_M_size )
			file->_M_position = _pos;
	}
	else
	{
		if( pos >= 0 && pos < (VFS_INT64)file->_M_size  )
			file->_M_position = pos;
	}

	return vfs_file_tell(file);
}

VFS_SIZE vfs_file_read( VFS_VOID* buf , VFS_SIZE size , VFS_SIZE count , vfs_file*fp )
{
	VFS_SIZE realsize;
    VFS_SIZE realcount;
	VFS_CHAR* p;
    VFS_CHAR* cursor;

	if(!fp || !buf || !size || !count)
		return 0;
    
    realcount = (VFS_SIZE)((fp->_M_size - fp->_M_position)/size);
    if( realcount <= 0  )
        return 0;

    realcount = realcount<count?realcount:count;

    p = (VFS_CHAR*)buf;
    cursor = &((VFS_CHAR*)fp->_M_buffer)[fp->_M_position];
    realsize = realcount* size;
    fp->_M_position += realsize;
    memcpy(p,cursor,(VFS_SIZE)realsize);
    return realcount;
}

VFS_SIZE vfs_file_write(VFS_VOID* buf , VFS_SIZE size , VFS_SIZE count , vfs_file*fp )
{
	VFS_SIZE realwrite;
    VFS_SIZE realcount;
    VFS_SIZE needsize;
	VFS_CHAR* p,*tmp;
	
	if( !fp || !buf || !size || !count )
		return 0;

    realcount = (VFS_SIZE)((fp->_M_size - fp->_M_position)/size);
    if(realcount < count )
    {
        needsize = (count - realcount)*size;
        if( fp->_M_size == 0 )
        {
            tmp = (VFS_VOID*)vfs_pool_malloc(needsize );
            if( tmp )
            {
                fp->_M_buffer = tmp;
                fp->_M_size = needsize;

                realcount = count;
            }
        }
        else
        {
            tmp = (VFS_VOID*)vfs_pool_realloc(fp->_M_buffer,(VFS_SIZE)(fp->_M_size + needsize) );
            if( tmp )
            {
                fp->_M_buffer = tmp;
                fp->_M_size += needsize;

                realcount = count;
            }
        }
        
    }

    if( realcount == 0 )
        return 0;

    p = (VFS_CHAR*)buf;
    realwrite = realcount*size;

	memcpy(&((VFS_CHAR*)fp->_M_buffer)[fp->_M_position],p,(VFS_SIZE)realwrite);
	fp->_M_position += realwrite;
	return realcount;
}


VFS_BOOL vfs_file_save(vfs_file* file,const VFS_CHAR* saveas)
{

	FILE* fp;
	VFS_UINT64 offset;

	VFS_UINT64 realsize ;
	VFS_CHAR buf[512+1];

	if( !file || !saveas  )
		return VFS_FALSE;
	
	fp = sfopen(saveas,"wb+");
	if( !fp )
		return VFS_FALSE;

	offset = vfs_file_tell(file);
    vfs_file_seek(file,0,SEEK_SET);
	while( !vfs_file_eof(file) )
	{
		realsize = vfs_file_read(buf,1,512,file);
		if( realsize > 0 )
		{
			buf[realsize] = 0;
			if( fwrite(buf,1,(VFS_SIZE)realsize,fp) != realsize )
			{
				VFS_SAFE_FCLOSE(fp);
				vfs_file_seek(file,offset,SEEK_SET);
				remove(saveas);
				return VFS_FALSE;
			}
		}
	}

    VFS_SAFE_FCLOSE(fp);
    vfs_file_seek(file,offset,SEEK_SET);
	return VFS_TRUE;
}
