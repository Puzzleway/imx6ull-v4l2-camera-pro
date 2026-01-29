
#ifndef _RENDER_H
#define _RENDER_H

#include <pic_operation.h>
#include <disp_manager.h>

/**********************************************************************
 * �������ƣ� PicZoom
 * ���������� ����ȡ����ֵ��������ͼƬ
 *            ע��ú���������ڴ���������ź��ͼƬ,�����Ҫ��free�����ͷŵ�
 *            "����ȡ����ֵ"��ԭ����ο�����"lantianyu520"������"ͼ�������㷨"
 * ��������� ptOriginPic - �ں�ԭʼͼƬ����������
 *            ptBigPic    - �ں����ź��ͼƬ����������
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int PicZoom(PT_PixelDatas ptOriginPic, PT_PixelDatas ptZoomPic);

/**********************************************************************
 * �������ƣ� PicMerge
 * ���������� ��СͼƬ�ϲ����ͼƬ��
 * ��������� iX,iY      - СͼƬ�ϲ����ͼƬ��ĳ������, iX/iYȷ�������������Ͻ�����
 *            ptSmallPic - �ں�СͼƬ����������
 *            ptBigPic   - �ں���ͼƬ����������
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���	      �޸�����
 * -----------------------------------------------
 * 2013/02/08	     V1.0	  Τ��ɽ	      ����
 ***********************************************************************/
int PicMerge(int iX, int iY, PT_PixelDatas ptSmallPic, PT_PixelDatas ptBigPic);
/**********************************************************************
 * �������ƣ� PicMergeRegion
 * ���������� ����ͼƬ��ĳ����, �ϲ�����ͼƬ��ָ������
 * ��������� iStartXofNewPic, iStartYofNewPic : ����ͼƬ��(iStartXofNewPic, iStartYofNewPic)���괦��ʼ�����������ںϲ�
 *            iStartXofOldPic, iStartYofOldPic : �ϲ�����ͼƬ��(iStartXofOldPic, iStartYofOldPic)����ȥ
 *            iWidth, iHeight                  : �ϲ�����Ĵ�С
 *            ptNewPic                         : ��ͼƬ
 *            ptOldPic                         : ��ͼƬ
 * ��������� ��
 * �� �� ֵ�� 0 - �ɹ�, ����ֵ - ʧ��
 * �޸�����        �汾��     �޸���          �޸�����
 * -----------------------------------------------
 * 2013/02/08        V1.0     Τ��ɽ          ����
 ***********************************************************************/
int PicMergeRegion(int iStartXofNewPic, int iStartYofNewPic, int iStartXofOldPic, int iStartYofOldPic, int iWidth, int iHeight, PT_PixelDatas ptNewPic, PT_PixelDatas ptOldPic);

#endif /* _RENDER_H */

