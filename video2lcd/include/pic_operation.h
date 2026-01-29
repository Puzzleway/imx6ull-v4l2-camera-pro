
#ifndef _PIC_OPERATION_H
#define _PIC_OPERATION_H

/* ͼƬ���������� */
typedef struct PixelDatas {
	int iWidth;   /* ����: һ���ж��ٸ����� */
	int iHeight;  /* �߶�: һ���ж��ٸ����� */
	int iBpp;     /* һ�������ö���λ����ʾ */
	int iLineBytes;  /* һ�������ж����ֽ� */
	int iTotalBytes; /* �����ֽ��� */ 
	unsigned char *aucPixelDatas;  /* �������ݴ洢�ĵط� */
}T_PixelDatas, *PT_PixelDatas;


#endif /* _PIC_OPERATION_H */

