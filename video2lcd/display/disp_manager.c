
#include <config.h>
#include <disp_manager.h>
#include <string.h>
#include <stdlib.h>    // 解决 malloc
#include <sys/ioctl.h> // 解决 ioctl
#include <unistd.h>    // 解决 getcwd (ui.c 中用到)

static PT_DispOpr g_ptDispOprHead;
static PT_DispOpr g_ptDefaultDispOpr;
static PT_VideoMem g_ptVideoMemHead;

/**********************************************************************
 * �������ƣ� RegisterDispOpr
 * ���������� ע��"��ʾģ��", ������֧�ֵ���ʾ�豸�Ĳ���������������
 * ��������� ptDispOpr - һ���ṹ��,�ں���ʾ�豸�Ĳ�������
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int RegisterDispOpr(PT_DispOpr ptDispOpr)
{
	PT_DispOpr ptTmp;

	if (!g_ptDispOprHead)
	{
		g_ptDispOprHead   = ptDispOpr;
		ptDispOpr->ptNext = NULL;
	}
	else
	{
		ptTmp = g_ptDispOprHead;
		while (ptTmp->ptNext)
		{
			ptTmp = ptTmp->ptNext;
		}
		ptTmp->ptNext	  = ptDispOpr;
		ptDispOpr->ptNext = NULL;
	}

	return 0;
}


/**********************************************************************
 * �������ƣ� ShowDispOpr
 * ���������� ��ʾ��������֧�ֵ�"��ʾģ��"
 * ��������� ��
 * ��������� ��
 * �� �� ֵ�� ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
void ShowDispOpr(void)
{
	int i = 0;
	PT_DispOpr ptTmp = g_ptDispOprHead;

	while (ptTmp)
	{
		printf("%02d %s\n", i++, ptTmp->name);
		ptTmp = ptTmp->ptNext;
	}
}

/**********************************************************************
 * �������ƣ� GetDispOpr
 * ���������� ��������ȡ��ָ����"��ʾģ��"
 * ��������� pcName - ����
 * ��������� ��
 * �� �� ֵ�� NULL   - ʧ��,û��ָ����ģ��, 
 *            ��NULL - ��ʾģ���PT_DispOpr�ṹ��ָ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
PT_DispOpr GetDispOpr(char *pcName)
{
	PT_DispOpr ptTmp = g_ptDispOprHead;
	
	while (ptTmp)
	{
		if (strcmp(ptTmp->name, pcName) == 0)
		{
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}

/**********************************************************************
 * �������ƣ� SelectAndInitDefaultDispDev
 * ���������� ��������ȡ��ָ����"��ʾģ��", �������ĳ�ʼ������, ��������
 * ��������� name - ����
 * ��������� ��
 * �� �� ֵ�� ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
void SelectAndInitDefaultDispDev(char *name)
{
	g_ptDefaultDispOpr = GetDispOpr(name);
	if (g_ptDefaultDispOpr)
	{
		g_ptDefaultDispOpr->DeviceInit();
		g_ptDefaultDispOpr->CleanScreen(0);
	}
}



/**********************************************************************
 * �������ƣ� GetDefaultDispDev
 * ���������� ����������SelectAndInitDefaultDispDevѡ������ʾģ��,
 *            ���������ظ���ʾģ��
 * ��������� ��
 * ��������� ��
 * �� �� ֵ�� ��ʾģ���PT_DispOpr�ṹ��ָ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
PT_DispOpr GetDefaultDispDev(void)
{
	return g_ptDefaultDispOpr;
}

/**********************************************************************
 * �������ƣ� GetDispResolution
 * ���������� �����ʹ�õ���ʾ�豸�ķֱ��ʺ�BPP
 * ��������� ��
 * ��������� piXres - ��X�ֱ���
 *            piYres - ��X�ֱ���
 *            piBpp  - ��BPP
 * �� �� ֵ�� 0  - �ɹ�
 *            -1 - ʧ��(δʹ��SelectAndInitDefaultDispDev��ѡ����ʾģ��)
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int GetDispResolution(int *piXres, int *piYres, int *piBpp)
{
	if (g_ptDefaultDispOpr)
	{
		*piXres = g_ptDefaultDispOpr->iXres;
		*piYres = g_ptDefaultDispOpr->iYres;
		*piBpp  = g_ptDefaultDispOpr->iBpp;
		return 0;
	}
	else
	{
		return -1;
	}
}

int GetVideoBufForDisplay(PT_VideoBuf ptFrameBuf)
{
    ptFrameBuf->iPixelFormat = (g_ptDefaultDispOpr->iBpp == 16) ? V4L2_PIX_FMT_RGB565 : \
                                   (g_ptDefaultDispOpr->iBpp == 32) ?  V4L2_PIX_FMT_RGB32 : \
                                           0;
    ptFrameBuf->tPixelDatas.iWidth  = g_ptDefaultDispOpr->iXres;
    ptFrameBuf->tPixelDatas.iHeight = g_ptDefaultDispOpr->iYres;
    ptFrameBuf->tPixelDatas.iBpp    = g_ptDefaultDispOpr->iBpp;
    ptFrameBuf->tPixelDatas.iLineBytes    = g_ptDefaultDispOpr->iLineWidth;
    ptFrameBuf->tPixelDatas.iTotalBytes   = ptFrameBuf->tPixelDatas.iLineBytes * ptFrameBuf->tPixelDatas.iHeight;
    ptFrameBuf->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;
    return 0;
}

void FlushPixelDatasToDev(PT_PixelDatas ptPixelDatas)
{
    g_ptDefaultDispOpr->ShowPage(ptPixelDatas);
}


/**********************************************************************
 * �������ƣ� AllocVideoMem
 * ���������� VideoMem: Ϊ�ӿ���ʾ�ٶ�,���������ڻ����й������ʾ��ҳ�������,
 *            (��������ΪVideoMem)
 *            ��ʾʱ�ٰ�VideoMem�е����ݸ��Ƶ��豸���Դ���
 * ��������� iNum
 * ��������� ��
 * �� �� ֵ�� 0  - �ɹ�
 *            -1 - ʧ��(δʹ��SelectAndInitDefaultDispDev��ѡ����ʾģ��)
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int AllocVideoMem(int iNum)
{
	int i;

	int iXres = 0;
	int iYres = 0;
	int iBpp  = 0;

	int iVMSize;
	int iLineBytes;

	PT_VideoMem ptNew;

	/* ȷ��VideoMem�Ĵ�С
	 */
	GetDispResolution(&iXres, &iYres, &iBpp);
	iVMSize = iXres * iYres * iBpp / 8;
	iLineBytes = iXres * iBpp / 8;

	/* �Ȱ��豸������framebuffer��������
	 * ����һ��T_VideoMem�ṹ��, ע������û�з��������tPixelDatas.aucPixelDatas
	 * ������tPixelDatas.aucPixelDatasָ����ʾ�豸��framebuffer
	 */
	ptNew = malloc(sizeof(T_VideoMem));
	if (ptNew == NULL)
	{
		return -1;
	}

	/* ָ��framebuffer */
	ptNew->tPixelDatas.aucPixelDatas = g_ptDefaultDispOpr->pucDispMem;
	
	ptNew->iID = 0;
	ptNew->bDevFrameBuffer = 1;        /* ��ʾ���VideoMem���豸������framebuffer, �����������������õ�VideoMem */
	ptNew->eVideoMemState  = VMS_FREE;
	ptNew->ePicState	   = PS_BLANK;
	ptNew->tPixelDatas.iWidth  = iXres;
	ptNew->tPixelDatas.iHeight = iYres;
	ptNew->tPixelDatas.iBpp    = iBpp;
	ptNew->tPixelDatas.iLineBytes  = iLineBytes;
	ptNew->tPixelDatas.iTotalBytes = iVMSize;

	if (iNum != 0)
	{
		/* �������Ҫ�������ڻ����VideoMem, 
		 * ���豸����framebuffer��Ӧ��VideoMem״̬����ΪVMS_USED_FOR_CUR,
		 * ��ʾ���VideoMem���ᱻ��Ϊ��������ȥ
		 */
		ptNew->eVideoMemState = VMS_USED_FOR_CUR;
	}
	
	/* �������� */
	ptNew->ptNext = g_ptVideoMemHead;
	g_ptVideoMemHead = ptNew;
	

	/*
	 * �������ڻ����VideoMem
	 */
	for (i = 0; i < iNum; i++)
	{
		/* ����T_VideoMem�ṹ�屾����"��framebufferͬ����С�Ļ���" */
		ptNew = malloc(sizeof(T_VideoMem) + iVMSize);
		if (ptNew == NULL)
		{
			return -1;
		}
		/* ��T_VideoMem�ṹ�����¼��������"��framebufferͬ����С�Ļ���" */
		ptNew->tPixelDatas.aucPixelDatas = (unsigned char *)(ptNew + 1);

		ptNew->iID = 0;
		ptNew->bDevFrameBuffer = 0;
		ptNew->eVideoMemState = VMS_FREE;
		ptNew->ePicState      = PS_BLANK;
		ptNew->tPixelDatas.iWidth  = iXres;
		ptNew->tPixelDatas.iHeight = iYres;
		ptNew->tPixelDatas.iBpp    = iBpp;
		ptNew->tPixelDatas.iLineBytes = iLineBytes;
		ptNew->tPixelDatas.iTotalBytes = iVMSize;

		/* �������� */
		ptNew->ptNext = g_ptVideoMemHead;
		g_ptVideoMemHead = ptNew;
	}
	
	return 0;
}

/**********************************************************************
 * �������ƣ� GetVideoMem
 * ���������� ���һ��ɲ�����VideoMem(�����ڴ洢Ҫ��ʾ������), 
 *            �������PutVideoMem���ͷ�
 * ��������� iID  - IDֵ,�ȳ������ڶ�VideoMem���ҵ�IDֵ��ͬ��
 *            bCur - 1��ʾ��ǰ��������Ҫʹ��VideoMem,�޷���ζ�Ҫ����һ��VideoMem
 *                   0��ʾ����Ϊ�˸Ľ����ܶ���ǰȡ��VideoMem,���Ǳ����
 * ��������� ��
 * �� �� ֵ�� NULL   - ʧ��,û�п��õ�VideoMem
 *            ��NULL - �ɹ�,����PT_VideoMem�ṹ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
PT_VideoMem GetVideoMem(int iID, int bCur)
{
	PT_VideoMem ptTmp = g_ptVideoMemHead;
	
	/* 1. ����: ȡ�����еġ�ID��ͬ��videomem */
	while (ptTmp)
	{
		if ((ptTmp->eVideoMemState == VMS_FREE) && (ptTmp->iID == iID))
		{
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}

	/* 2. ���ǰ�治�ɹ�, ȡ��һ�����еĲ�������û������(ptVideoMem->ePicState = PS_BLANK)��VideoMem */
	ptTmp = g_ptVideoMemHead;
	while (ptTmp)
	{
		if ((ptTmp->eVideoMemState == VMS_FREE) && (ptTmp->ePicState == PS_BLANK))
		{
			ptTmp->iID = iID;
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}	
	
	/* 3. ���ǰ�治�ɹ�: ȡ������һ�����е�VideoMem */
	ptTmp = g_ptVideoMemHead;
	while (ptTmp)
	{
		if (ptTmp->eVideoMemState == VMS_FREE)
		{
			ptTmp->iID = iID;
			ptTmp->ePicState = PS_BLANK;
			ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}

    /* 4. ���û�п��е�VideoMem����bCurΪ1, ��ȡ������һ��VideoMem(�������Ƿ����) */
    if (bCur)
    {
    	ptTmp = g_ptVideoMemHead;
    	ptTmp->iID = iID;
    	ptTmp->ePicState = PS_BLANK;
    	ptTmp->eVideoMemState = bCur ? VMS_USED_FOR_CUR : VMS_USED_FOR_PREPARE;
    	return ptTmp;
    }
    
	return NULL;
}

/**********************************************************************
 * �������ƣ� PutVideoMem
 * ���������� ʹ��GetVideoMem��õ�VideoMem, ����ʱ��PutVideoMem�ͷŵ�
 * ��������� ptVideoMem - ʹ����ϵ�VideoMem
 * ��������� ��
 * �� �� ֵ�� ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
void PutVideoMem(PT_VideoMem ptVideoMem)
{
	ptVideoMem->eVideoMemState = VMS_FREE;  /* ����VideoMem״̬Ϊ���� */
    if (ptVideoMem->iID == -1)
    {
        ptVideoMem->ePicState = PS_BLANK;  /* ��ʾ���������û������ */
    }
}

/**********************************************************************
 * �������ƣ� GetDevVideoMem
 * ���������� �����ʾ�豸���Դ�, �ڸ��Դ��ϲ����Ϳ���ֱ����LCD����ʾ����
 * ��������� ��
 * ��������� ��
 * �� �� ֵ�� �Դ��Ӧ��VideoMem�ṹ��ָ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
PT_VideoMem GetDevVideoMem(void)
{
	PT_VideoMem ptTmp = g_ptVideoMemHead;
	
	while (ptTmp)
	{
		if (ptTmp->bDevFrameBuffer)
		{
			return ptTmp;
		}
		ptTmp = ptTmp->ptNext;
	}
	return NULL;
}


/**********************************************************************
 * �������ƣ� ClearVideoMem
 * ���������� ��VideoMem���ڴ�ȫ����Ϊĳ����ɫ
 * ��������� ptVideoMem - VideoMem�ṹ��ָ��, �ں�Ҫ�������ڴ�
 *            dwColor    - ����Ϊ����ɫ
 * ��������� ��
 * �� �� ֵ�� ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
void ClearVideoMem(PT_VideoMem ptVideoMem, unsigned int dwColor)
{
	unsigned char *pucVM;
	unsigned short *pwVM16bpp;
	unsigned int *pdwVM32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;
	int i = 0;

	pucVM	   = ptVideoMem->tPixelDatas.aucPixelDatas;
	pwVM16bpp  = (unsigned short *)pucVM;
	pdwVM32bpp = (unsigned int *)pucVM;

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
		case 8:
		{
			memset(pucVM, dwColor, ptVideoMem->tPixelDatas.iTotalBytes);
			break;
		}
		case 16:
		{
			/* �ȸ���32λ��dwColor�����16λ��wColor16bpp */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			while (i < ptVideoMem->tPixelDatas.iTotalBytes)
			{
				*pwVM16bpp	= wColor16bpp;
				pwVM16bpp++;
				i += 2;
			}
			break;
		}
		case 32:
		{
			while (i < ptVideoMem->tPixelDatas.iTotalBytes)
			{
				*pdwVM32bpp = dwColor;
				pdwVM32bpp++;
				i += 4;
			}
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", ptVideoMem->tPixelDatas.iBpp);
			return;
		}
	}

}


/**********************************************************************
 * �������ƣ� ClearVideoMemRegion
 * ���������� ��VideoMem���ڴ��ָ������ȫ����Ϊĳ����ɫ
 * ��������� ptVideoMem - VideoMem�ṹ��ָ��, �ں�Ҫ�������ڴ�
 *            ptLayout   - ��������, ָ�������Ͻ�,���ڽǵ�����
 *            dwColor    - ����Ϊ����ɫ
 * ��������� ��
 * �� �� ֵ�� ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
void ClearVideoMemRegion(PT_VideoMem ptVideoMem, PT_Layout ptLayout, unsigned int dwColor)
{
	unsigned char *pucVM;
	unsigned short *pwVM16bpp;
	unsigned int *pdwVM32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;
	int iX;
	int iY;
    int iLineBytesClear;
    int i;

	pucVM	   = ptVideoMem->tPixelDatas.aucPixelDatas + ptLayout->iTopLeftY * ptVideoMem->tPixelDatas.iLineBytes + ptLayout->iTopLeftX * ptVideoMem->tPixelDatas.iBpp / 8;
	pwVM16bpp  = (unsigned short *)pucVM;
	pdwVM32bpp = (unsigned int *)pucVM;

    iLineBytesClear = (ptLayout->iBotRightX - ptLayout->iTopLeftX + 1) * ptVideoMem->tPixelDatas.iBpp / 8;

	switch (ptVideoMem->tPixelDatas.iBpp)
	{
		case 8:
		{
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
    			memset(pucVM, dwColor, iLineBytesClear);
                pucVM += ptVideoMem->tPixelDatas.iLineBytes;
            }
			break;
		}
		case 16:
		{
			/* �ȸ���32λ��dwColor�����16λ��wColor16bpp */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
                i = 0;
                for (iX = ptLayout->iTopLeftX; iX <= ptLayout->iBotRightX; iX++)
    			{
    				pwVM16bpp[i++]	= wColor16bpp;
    			}
                pwVM16bpp = (unsigned short *)((unsigned int)pwVM16bpp + ptVideoMem->tPixelDatas.iLineBytes);
            }
			break;
		}
		case 32:
		{
            for (iY = ptLayout->iTopLeftY; iY <= ptLayout->iBotRightY; iY++)
            {
                i = 0;
                for (iX = ptLayout->iTopLeftX; iX <= ptLayout->iBotRightX; iX++)
    			{
    				pdwVM32bpp[i++]	= dwColor;
    			}
                pdwVM32bpp = (unsigned int *)((unsigned int)pdwVM32bpp + ptVideoMem->tPixelDatas.iLineBytes);
            }
			break;
		}
		default :
		{
			DBG_PRINTF("can't support %d bpp\n", ptVideoMem->tPixelDatas.iBpp);
			return;
		}
	}

}

/**********************************************************************
 * �������ƣ� DisplayInit
 * ���������� ע����ʾ�豸
 * ��������� ��
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int DisplayInit(void)
{
	int iError;
	
	iError = FBInit();

	return iError;
}

