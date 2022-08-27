#include"dataSet.h"
#include<string>
#include<vector>
#include <io.h>
using namespace std;

//������Ŀ¼�µ�.jpgͼƬ
void load_data_from_folder(std::string path, std::string type, std::vector<std::string>& list_images, std::vector<int>& list_labels, int label)
{
    long long hFile = 0; //���
    struct _finddata_t fileInfo;
    std::string pathName;
    if ((hFile = _findfirst(pathName.assign(path).append("\\*.*").c_str(), &fileInfo)) == -1)
    {
        return;
    }
    do
    {
        const char* s = fileInfo.name;
        const char* t = type.data();

        if (fileInfo.attrib & _A_SUBDIR) //�����ļ���
        {
            //�������ļ����е��ļ�(��)
            if (strcmp(s, ".") == 0 || strcmp(s, "..") == 0) //���ļ���Ŀ¼��.����..
                continue;
            std::string sub_path = path + "\\" + fileInfo.name;
            label++;
            load_data_from_folder(sub_path, type, list_images, list_labels, label);

        }
        else //�ж��ǲ��Ǻ�׺Ϊtype�ļ�
        {
            if (strstr(s, t))
            {
                std::string image_path = path + "\\" + fileInfo.name;
                list_images.push_back(image_path);
                list_labels.push_back(label);
            }
        }
    } while (_findnext(hFile, &fileInfo) == 0);
    return;
}