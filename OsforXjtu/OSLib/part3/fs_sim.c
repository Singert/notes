#include<stdio.h>
#include"string.h"
#include"stdlib.h"
#include"time.h"
#include<sys/ioctl.h>
#include<termios.h>
#include<unistd.h>
#include <stdint.h>

#define blocks 4611 //总块数：1+1+1+512+4096
#define blocksize 512 //每块字节数
#define inodesize 64 //索引结点大小
#define data_begin_block 515 //数据开始块1+1+1+512
#define dirsize 261 //目录体最大长度
#define EXT_NAME_LEN 255 //文件名长度
#define DISK_START 0
#define BLOCK_BITMAP 512
#define INODE_BITMAP 1024
#define INODE_TABLE 1536
#define PATH "virdisk" //文件系统

/******************************    数据结构    ****************************************/
typedef struct ext2_group_desc//组描述符 68字节
{
    char bg_volume_name[16]; //卷名
    uint16_t bg_block_bitmap; //保存块位图的块号
    uint16_t bg_inode_bitmap; //保存索引结点位图的块号
    uint16_t bg_inode_table; //索引结点表的起始块号
    uint16_t bg_free_blocks_count; //本组空闲块的个数
    uint16_t bg_free_inodes_count; //本组空闲索引结点的个数
    uint16_t bg_used_dirs_count; //本组目录的个数
    char psw[16];//password
    char bg_pad[24];
}ext2_group_desc;

typedef struct ext2_inode //索引结点64字节
{
    uint16_t i_mode;//文件类型及访问权限
    uint16_t i_blocks; //文件的数据块个数
    uint32_t i_size;//该索引结点指向的数据块所使用的大小(字节)
    time_t i_atime; //访问时间
    time_t i_ctime; //创建时间
    time_t i_mtime; //修改时间
    time_t i_dtime; //删除时间
    uint16_t i_block[8];//指向数据块的指针
    char i_pad[8]; //填充
}ext2_inode;

typedef struct ext2_dir_entry//目录体 最大261字节 最小7字节
{
    uint16_t inode;//索引节点号，从1开始
    uint16_t rec_len;//目录项长度
    uint8_t name_len;//文件名长度
    uint8_t file_type;//文件类型(1:普通文件，2:目录…)
    char name[EXT_NAME_LEN];
    char i_pad[14]; //填充
}ext2_dir_entry;

//定义全局变量
//缓冲区用于读出和写入数据块的相关内容
ext2_group_desc group_desc;//组描述符
ext2_inode inode;//索引结点缓冲区，只在初始化的时候用到
ext2_dir_entry dir;//目录体缓冲区
FILE *fp;//文件指针
uint16_t last_alloc_inode=0;//上次分配的索引节点号
uint16_t last_alloc_block=0;//上次分配的数据块号
uint16_t currentdir_inode;  //当前目录的索引结点号,很重要，每次用这个来获取当前目录
uint16_t fopen_table[16] ; /*文件打开表，最多可以同时打开16个文件*/
char current_path[256];   	 /*当前路径(字符串) */
uint8_t bitbuf[512]; //位图缓冲区 512字节
uint8_t block_buffer[512];//数据块缓冲区

/******************************    底层    ****************************************/
void update_group_desc() /*将内存中的组描述符更新到"硬盘"*/
{
    fseek(fp,DISK_START,SEEK_SET);
    fwrite(&group_desc,sizeof(ext2_group_desc),1,fp);
}
void reload_group_desc() /*载入可能已更新的组描述符*/
{
    fseek(fp,DISK_START,SEEK_SET);
    fread(&group_desc,sizeof(ext2_group_desc),1,fp);
}	
void reload_inode_entry(uint16_t i,ext2_inode* inode_i)  /*载入特定的索引结点*/
{
    fseek(fp,INODE_TABLE+(i-1)*inodesize,SEEK_SET);
    fread(inode_i,inodesize,1,fp);
}
void update_inode_entry(uint16_t i,ext2_inode* inode_i) /*更新特定的索引结点*/
{
    fseek(fp,INODE_TABLE+(i-1)*inodesize,SEEK_SET);
    fwrite(inode_i,inodesize,1,fp);
}

void update_block_bitmap()//更新block位图 
{
    fseek(fp,BLOCK_BITMAP,SEEK_SET);
    fwrite(bitbuf,blocksize,1,fp);
}
void reload_block_bitmap()//载入block位图 
{
    fseek(fp,BLOCK_BITMAP,SEEK_SET);
    fread(bitbuf,blocksize,1,fp);
}
void update_inode_bitmap()//更新inode位图 
{
    fseek(fp,INODE_BITMAP,SEEK_SET);
    fwrite(bitbuf,blocksize,1,fp);
}
void reload_inode_bitmap()//载入inode位图 
{
    fseek(fp,INODE_BITMAP,SEEK_SET);
    fread(bitbuf,blocksize,1,fp);
}
void reload_block_entry(uint16_t i,uint8_t* block)
{//将第i个数据块读到缓冲区block中
    fseek(fp,data_begin_block*blocksize+i*blocksize,SEEK_SET);
    fread(block,blocksize,1,fp);
}
void update_block_entry(uint16_t i,uint8_t* block)
{//将缓冲区block的内容写入第i个数据块
    fseek(fp,data_begin_block*blocksize+i*blocksize,SEEK_SET);
    fwrite(block,blocksize,1,fp);
}

uint16_t alloc_block()//分配一个数据块,返回数据块号 
{
    uint16_t cur=0;
    uint8_t con=128;
    int offset=0;//数据块在数据块位图中某一字节的第几位
    if(group_desc.bg_free_blocks_count==0)
    {
        printf("There is no block to be alloced!\n");
        return -1;
    }
    reload_block_bitmap();
    cur=cur/8;
    while(bitbuf[cur]==255)
    {
        if(cur==511)
        {
            printf("ERROR\n");
            return -1;
        }
        else cur++;
    }
    while(bitbuf[cur]&con)
    {
        con = con >> 1;;//右移一位
        offset++;
    }
    bitbuf[cur]=bitbuf[cur]+con;
    last_alloc_block=cur*8+offset;
    update_block_bitmap();
    group_desc.bg_free_blocks_count--;
    update_group_desc();
    return last_alloc_block;
}
void free_block(uint16_t block_num)
{
    reload_block_bitmap();
    uint8_t con=128;
    int offset;//数据块在数据块位图中某一字节的第几位
    int byte_pos=block_num/8;//数据块在数据块位图中第几个字节中
    for(offset=0;offset<block_num%8;offset++)
    {
        con = con >> 1;//右移一位
    }
    if(bitbuf[byte_pos]&con)
    {
        bitbuf[byte_pos]=bitbuf[byte_pos]-con;
        reload_block_entry(block_num,block_buffer);
        for(int i=0;i<blocksize;i++)
            block_buffer[i]=0;
        update_block_entry(block_num,block_buffer);//将数据块清零
        update_block_bitmap();
        group_desc.bg_free_blocks_count++;
        update_group_desc();
    }
    else{
        printf("%d:the block to be freed has not been allocated.\n",block_num);
    }
}

/*根据多级索引机制获取数据块号*/
uint16_t get_index_one(uint16_t i,uint16_t first_index)  /*返回一级索引的数据块号*/
{//first_index=inode.i_block[6]
    if(i>=6&&i<6+256)
    {
        uint8_t tmp_block[512];
        uint16_t block_num;
        reload_block_entry(first_index,tmp_block);
        memcpy(&block_num,tmp_block+2*(i-6),2);
        return block_num;
    }
    else printf("错误使用一级索引");
}
uint16_t get_index_two(uint16_t i,uint16_t two_index)  /*返回二级索引的数据块号*/
{//two_index=inode.i_block[7]
    if(i>=6+256&&i<4096)
    {
        int block_num,tmp_num;
        uint8_t tmp_block1[512],tmp_block2[512];
        reload_block_entry(two_index,tmp_block1);
        memcpy(&tmp_num,tmp_block1+2*(i-6-256)/256,2);
        reload_block_entry(tmp_num,tmp_block2);
        memcpy(&block_num,tmp_block2+2*((i-6-256)%256),2);
        return block_num;
    }
    else printf("错误使用二级索引");
}
void reload_block_i(uint16_t i,unsigned char tmp_block[512],ext2_inode tmp_inode)
{
    if(i<6)
        reload_block_entry(tmp_inode.i_block[i],tmp_block);
    else if(i>=6&&i<6+256)
        reload_block_entry(get_index_one(i,tmp_inode.i_block[6]),tmp_block);
    else 
        reload_block_entry(get_index_two(i,tmp_inode.i_block[7]),tmp_block);
}
void reload_dir_i(uint16_t i,ext2_dir_entry* dir_i,uint16_t offset,ext2_inode tmp_inode)
{//载入当前索引节点第i个数据块中的某个目录项到缓冲区中
    uint8_t tmp_block[512];
    reload_block_i(i,tmp_block,tmp_inode);
    memcpy(dir_i,tmp_block+offset,dirsize);
}
void update_dir_i(uint16_t i,ext2_dir_entry* dir_i,uint16_t offset,ext2_inode tmp_inode)
{//将某个目录项写入当前索引结点的某个数据块中
    uint8_t tmp_block[512];
    memcpy(tmp_block+offset,dir_i,dir_i->rec_len);
    if(i<6)
    {
        reload_block_entry(tmp_inode.i_block[i],tmp_block);
        memcpy(tmp_block+offset,dir_i,dir_i->rec_len);
        update_block_entry(tmp_inode.i_block[i],tmp_block);
    }
        
    else if(i>=6&&i<6+256)
    {
        reload_block_entry(get_index_one(i,tmp_inode.i_block[6]),tmp_block);
        memcpy(tmp_block+offset,dir_i,dir_i->rec_len);
        update_block_entry(get_index_one(i,tmp_inode.i_block[6]),tmp_block);
    }
    else 
    {
        reload_block_entry(get_index_two(i,tmp_inode.i_block[7]),tmp_block);
        memcpy(tmp_block+offset,dir_i,dir_i->rec_len);
        update_block_entry(get_index_two(i,tmp_inode.i_block[7]),tmp_block);
    }
}
void update_index_one(uint16_t i,uint16_t block_num,ext2_inode* tmp_inode)
{//将当前结点的第i个数据块号写入一级索引块
    if(i==6)
    {
        tmp_inode->i_block[6]=alloc_block();
    }
    if(i>=6&&i<6+256)
    {
        uint8_t tmp_block[512];
        reload_block_entry(tmp_inode->i_block[6],tmp_block);
        memcpy(tmp_block+(i-6)*2,&block_num,2);
        update_block_entry(tmp_inode->i_block[6],tmp_block);
    }
    else printf("错误使用一级索引");
}
void update_index_two(uint16_t i,uint16_t block_num,ext2_inode* tmp_inode)  
{//将当前结点的第i个数据块号写入二级索引块
    if(i>=6+256&&i<4096)
    {
        int tmp_num;
        if(i==6+256)
        {
            tmp_inode->i_block[7]=alloc_block();
        }
        if((i-6)%256==0)
        {
            tmp_num=alloc_block();
            uint8_t tmp_block[512];
            reload_block_entry(tmp_inode->i_block[7],tmp_block);
            memcpy(tmp_block+2*(i-6-256)/256,&tmp_num,2);
            update_block_entry(tmp_inode->i_block[7],tmp_block);
        }
        else{
            uint8_t tmp_block[512];
            reload_block_entry(tmp_inode->i_block[7],tmp_block);
            memcpy(&tmp_num,tmp_block+2*(i-6-256)/256,2);
        }
        if(i>=6+256&&i<4096)
        {
            uint8_t tmp_block[512];
            reload_block_entry(tmp_num,tmp_block);
            memcpy(tmp_block+2*((i-6-256)%256),&block_num,2);
            update_block_entry(tmp_num,tmp_block);
        }
    }
    else printf("错误使用二级索引");
}
void update_inode_newblock(uint16_t block_num,ext2_inode* tmp_inode)
{//将新分配的数据块记录到索引结点的信息中
    if(tmp_inode->i_blocks<6)
    {
        tmp_inode->i_block[tmp_inode->i_blocks]=block_num;
    }
    else if(tmp_inode->i_blocks>=6&&tmp_inode->i_blocks<6+256)
    {
        update_index_one(tmp_inode->i_blocks,block_num,tmp_inode);
    }
    else if(tmp_inode->i_blocks>=6+256)
    {
        update_index_two(tmp_inode->i_blocks,block_num,tmp_inode);
    }
    tmp_inode->i_blocks++;
}
void delete_inode_allblock(uint16_t inode_num)
{
    ext2_inode tmp_inode;
    reload_inode_entry(inode_num,&tmp_inode);
    for(int i=0;i<tmp_inode.i_blocks;i++)
    {
        if(i<6)
            free_block(tmp_inode.i_block[i]);
        else if(i>=6&&i<6+256)
            free_block(get_index_one(i,tmp_inode.i_block[6]));
        else if(i>=6+256)
            free_block(get_index_one(i,tmp_inode.i_block[7]));
    }
    if(tmp_inode.i_blocks>6)
        free_block(tmp_inode.i_block[6]);//删除一级索引块
    if(tmp_inode.i_blocks>6+256)
    {
        for(int j=0;6+256+j*256<tmp_inode.i_blocks;j++)
        {//删除二级索引指向的索引块
            uint8_t tmp_block1[512];
            int tmp_num;
            reload_block_entry(tmp_inode.i_block[7],tmp_block1);
            memcpy(&tmp_num,tmp_block1+j*2,2);
            free_block(tmp_num);
        }
        free_block(tmp_inode.i_block[7]);//删除二级索引块
    }
        
    tmp_inode.i_blocks=0;  
    update_inode_entry(inode_num,&tmp_inode);
}

uint16_t get_inode()//分配一个inode,返回序号 
{
    uint16_t cur=1;
    uint8_t con=128;//0b10000000
    int flag=0;
    if(group_desc.bg_free_inodes_count==0)
    {
        printf("There is no Inode to be alloced!\n");
        return 0;
    }
    reload_inode_bitmap();//将索引节点位图读入缓冲区
    cur=(cur-1)/8;//索引结点位图中相应字节的位置
    while(bitbuf[cur]==255)//寻找可用的字节
    {
        if(cur==511)
        {
            printf("ERROR\n");
            return 0;
        }
        else cur++;
    }
    while(bitbuf[cur]&con)// 寻找位图中的第一个未被占用的位
    {
        con=con/2;//相当于右移一位
        flag++;//标志该字节的第几位，从0开始
    }
    bitbuf[cur]=bitbuf[cur]+con;
    last_alloc_inode=cur*8+flag+1;
    update_inode_bitmap();//进行了索引结点位图的更新
    group_desc.bg_free_inodes_count--;
    update_group_desc();
    return last_alloc_inode;
}
void free_inode(uint16_t inode_num)
{
    reload_inode_bitmap();
    uint8_t con=128;
    int offset;//索引结点在索引结点位图中某一字节的第几位
    int byte_pos=(inode_num-1)/8;//索引结点在索引结点位图中第几个字节中
    for(offset=0;offset<(inode_num-1)%8;offset++)
    {
        con = con >> 1;//右移一位
    }
    if(bitbuf[byte_pos]&con)
    {
        ext2_inode tmp_inode;
        bitbuf[byte_pos]=bitbuf[byte_pos]-con;
        reload_inode_entry(inode_num,&tmp_inode);
        time_t now;
        time(&now);
        tmp_inode.i_dtime=now;
        update_inode_entry(inode_num,&tmp_inode);
        //不必将索引结点清零，修改索引结点位图就代表已释放，这样可以使用删除时间追踪文件系统中的操作
        update_block_bitmap();
        group_desc.bg_free_inodes_count++;
        update_group_desc();
    }
    else{
        printf("the inoded to be freed has not been allocated.\n");
    }
}

void dir_prepare(ext2_inode* inode_i,uint16_t last,uint16_t cur)
{//新建目录时当前目录和上一级目录的初始化
    uint8_t tmp_block[512];
    reload_block_entry(inode_i->i_block[0],tmp_block);
    ext2_dir_entry dir1,dir2;
    memcpy(&dir1,tmp_block,dirsize);
    dir1.inode=cur;
    dir1.rec_len=8;
    inode_i->i_size+=dir1.rec_len;
    dir1.name_len=1;
    dir1.file_type=2;
    strcpy(dir1.name,".");//当前目录
    memcpy(tmp_block,&dir1,dir1.rec_len);
    
    uint16_t offset=dir1.rec_len;
    memcpy(&dir2,tmp_block+offset,dirsize);
    dir2.inode=last;
    dir2.rec_len=9;
    inode_i->i_size+=dir2.rec_len;
    dir2.name_len=2;
    dir2.file_type=2;
    strcpy(dir2.name,"..");//上级目录
    memcpy(tmp_block+offset,&dir2,dir2.rec_len);
    update_block_entry(inode_i->i_block[0],tmp_block);

}

int test_fd(uint16_t inode_num);
void close_file(unsigned char name[255]);
uint16_t search_filename(char tmp[255],uint8_t file_type,ext2_inode* cur_inode,int flag)
{//在当前目录中查找文件或子目录，返回索引节点号,flag=1时删除目录项,flag=2时目录为空才删除
//如果file_type=0，则不确定是查找目录或文件，同名即可
    for(int i=0;i<cur_inode->i_blocks;i++)
    {
        reload_dir_i(i,&dir,0,*cur_inode);
        int offset=0;//读出的目录项的实际长度累加
        while(dir.rec_len!=0)//若读出的目录项长度为0则结束循环
        {
            if(dir.rec_len<0)printf("wrong len\n");
            if(dir.inode)
            {
                if(dir.file_type==file_type||file_type==0)
                {
                    if(strcmp(dir.name,tmp)==0)
                    {
                        ext2_inode tmp_inode;
                        reload_inode_entry(dir.inode,&tmp_inode);
                        if(flag==1||((flag==2)&&(tmp_inode.i_size==17)))
                        {
                            if(file_type==1)
                            {
                                if(test_fd(dir.inode)) 
                                {
                                    int pos;
                                    for(pos=0;pos<16;pos++)
                                    {//关闭文件
                                        if(fopen_table[pos]==dir.inode)
                                        {
                                            fopen_table[pos]=0;
                                            printf("File %s is closed successfully!\n",dir.name);
                                        }
                                    }
                                }
                            }
                            uint16_t num=dir.inode;
                            dir.inode=0;
                            update_dir_i(i,&dir,offset,*cur_inode);
                            cur_inode->i_size-=dir.rec_len;
                            group_desc.bg_used_dirs_count--;
                            update_group_desc();
                            return num;
                        }
                        else
                            return dir.inode;
                    }
                }
            }
            offset+=dir.rec_len;
            if(512-offset<7)break;//此数据块已读完(目录项长度至少7字节)
            reload_dir_i(i,&dir,offset,*cur_inode);
        }
    }
    if(file_type==1) printf("No such file named %s!\n",tmp);
    else if(file_type==2) printf("No such directory named %s!\n",tmp);
    return 0;
}

int test_fd(uint16_t inode_num)
{
    int fopen_table_point;
    for(fopen_table_point=0;fopen_table_point<16;fopen_table_point++)
    {
        if(fopen_table[fopen_table_point]==inode_num)   break;
    }
    if(fopen_table_point==16)return 0;
    return 1;
}

/******************************    初始化    ****************************************/
void initialize_disk() 
{
    printf("creating ext2 file system\n");
    last_alloc_inode=1;
    last_alloc_block=0;
    for(int i=0;i<16;i++)fopen_table[i]=0;//清空文件打开表
    // 创建模拟文件系统的虚拟硬盘文件
    time_t now;
    time(&now);
    fp=fopen(PATH, "wb+");
    if (fp == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    for(uint16_t i=0;i<blocksize;i++)
        block_buffer[i]=0;
    for(uint16_t i=0;i<blocks;i++)
    {
        fseek(fp,DISK_START+i*blocksize,SEEK_SET);
        fwrite(block_buffer,blocksize,1,fp);
    }//清空文件，即清空磁盘全部用0填充

    int a=3;
    fseek(fp,blocksize/2,SEEK_SET);
    fwrite(&a,4,1,fp);

    strcpy(current_path,"[root@ /");  //路径名
    //初始化组描述符
    strcpy(group_desc.bg_volume_name, "EXT2_DISK");
    group_desc.bg_block_bitmap = 1;//保存块位图的块号
    group_desc.bg_inode_bitmap = 2;//保存索引节点位图的块号
    group_desc.bg_inode_table = 3;//索引节点表的起始块号
    group_desc.bg_free_blocks_count=4096;
    group_desc.bg_free_inodes_count=4096;
    group_desc.bg_used_dirs_count=1;
    strcpy(group_desc.psw,"admin");
    update_group_desc(); //更新组描述符内容,将组描述符写入文件

    currentdir_inode=get_inode();//分配索引结点并初始化索引结点位图
    reload_inode_entry(currentdir_inode,&inode);
    //初始化索引结点表，添加第一个索引节点
    inode.i_mode = 15;// 目录访问权限类型默认rwx
    inode.i_blocks = 0;
    inode.i_size = 0; 
    inode.i_atime = now;
    inode.i_ctime = now;
    inode.i_mtime = now;
    inode.i_dtime = 0; // 删除时间为0表示未删除
    inode.i_block[0] = alloc_block();//分配数据块并更新数据块位图
    inode.i_blocks++;

    //创建当前目录和上级目录，并将两个目录项写入虚拟磁盘
    dir_prepare(&inode,currentdir_inode,currentdir_inode);
    update_inode_entry(currentdir_inode,&inode);// 将根目录的索引结点写入虚拟硬盘

    if(fclose(fp))printf("error");
    printf("installed\n");
}

void initialize_memory() 
{
    last_alloc_inode=1;
    last_alloc_block=0;
    for(int i=0;i<16;i++)fopen_table[i]=0;
    strcpy(current_path,"[root@ /");
    fp=fopen(PATH, "rb");
    
    if(fp==NULL)
    {
        printf("The File system does not exist!\n");
        fclose(fp);
        initialize_disk();
        return ;
    }
    currentdir_inode=1;
    reload_group_desc();
    if(fclose(fp))printf("error");
}

/******************************    命令层    ****************************************/
/*----------   目录   ----------*/
void dir_ls()
{
    fp=fopen(PATH,"rb+");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);
    if(!(cur_inode.i_mode&4)) //检查访问权限
    {
        printf("No permission to list items in current directory!\n");
        return;
    }
    
    printf("mode    type        size      CreateTime                  LastAccessTime              ModifyTime                  name\n");
    time_t now;
    time(&now);
    for(uint16_t i=0;i<cur_inode.i_blocks;i++)
    {
        
        reload_dir_i(i,&dir,0,cur_inode);
        int offset=0;//读出的目录项的实际长度
        while(dir.rec_len!=0)//若读出的目录项长度为0则结束循环
        {
            if(dir.rec_len<0)printf("wrong len\n");
            if(dir.inode)
            {
                
                ext2_inode tmp_inode;
                reload_inode_entry(dir.inode,&tmp_inode);//遍历当前目录下文件的索引结点
                if(tmp_inode.i_mode&4) printf("r_");
                else printf("__");
                if(tmp_inode.i_mode&2) printf("w_");
                else printf("__");
                if(tmp_inode.i_mode&1) printf("x   ");
                else printf("_   ");
                switch (dir.file_type)
                {
                case 1:printf("%-12s","File");printf("%-5uB   ",tmp_inode.i_size);break;
                case 2:printf("%-12s","Directory");printf("%-5uB   ",tmp_inode.i_size);break;
                default:printf("%-12s","others");break;
                }
                
                char timestr[100];
                strcpy(timestr," ");
                strcat(timestr,asctime(localtime(&tmp_inode.i_ctime)));
                strcat(timestr,"   ");
                strcat(timestr,asctime(localtime(&tmp_inode.i_atime)));
                strcat(timestr,"   ");
                strcat(timestr,asctime(localtime(&tmp_inode.i_mtime)));
                strcat(timestr,"   ");
                for(int j=0;j<strlen(timestr);j++)
                    if(timestr[j]=='\n') timestr[j]=' ';
                printf("%s", timestr);
                printf("%-s\n",dir.name);
            }
            offset+=dir.rec_len;
            if(512-offset<7)break;//此数据块已读完,剩下不足7字节为外碎片
            reload_dir_i(i,&dir,offset,cur_inode);
        }
    }
    cur_inode.i_atime=now;
    update_inode_entry(currentdir_inode,&cur_inode);
    fclose(fp);
}

void cd(char path_name[256])
{
    fp=fopen(PATH,"rb");
    uint16_t find_inode;//当前搜索的文件或目录的索引结点
    char search_path[256],new_path[256];//分别用于截断输入的路径和存储新路径
    strcpy(search_path,path_name);
    if(path_name[0]=='/'){//绝对路径
        find_inode=1;//根目录的索引节点号
        for(int i=0;path_name[i];i++)//此时正好能将终止符也复制
            search_path[i]=path_name[i+1];
        strcpy(new_path,"[root@ /");
    }else{//相对路径
        find_inode=currentdir_inode;//当前目录的索引节点号
        strcpy(search_path,path_name);
        strcpy(new_path,current_path);
    }

    char *token;
    // 使用strtok函数分割字符串
    token = strtok(search_path, "/");
    // 循环输出分割后的路径部分
    while (token != NULL) {
        ext2_inode cur_inode;
        reload_inode_entry(find_inode,&cur_inode);
        find_inode=search_filename(token,2,&cur_inode,0);
        ext2_inode tmp_inode;
        reload_inode_entry(find_inode,&tmp_inode);
        if(!(tmp_inode.i_mode&1)) //检查访问权限
        {
            printf("No permission to access to the directory!\n");
            return;
        }
        if(find_inode)
        {
            if(strcmp(path_name,".")==0){
                break;
            }
            
            if(strcmp(path_name,"..")==0)
            {//返回上级目录
                char *lastc = strrchr(new_path, '/');
                if (lastc != NULL) 
                    *lastc = '\0';
                lastc = strrchr(new_path, '/');
                if (lastc != NULL) // 找到最后一个斜杠，截断字符串
                    *lastc = '\0';
                strcat(new_path,"/");
                break;
            }
            //继续按路径寻找目录
            strcat(new_path,dir.name);
            strcat(new_path,"/");
        }
        else{
            fclose(fp);
            return;
        }
        token = strtok(NULL, "/");
    }
    currentdir_inode=find_inode;
    strcpy(current_path,new_path);
    fclose(fp); 
}

void create(uint8_t type_num,char name[255])
{
    fp=fopen(PATH,"rb+");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);
    if((!(cur_inode.i_mode&1))||(!(cur_inode.i_mode&2))) //检查访问权限
    {
        printf("No permission to create file/directory in current directory!\n");
        return;
    }
    if(strlen(name)>255)
        name[255]='\0';
    int new_len=7+strlen(name);
    int flag=0;//是否找到已存在且大小足够的目录项
    ext2_dir_entry tmp_dir;
    uint16_t num;//分配给新文件的目录项在当前目录的第几个数据块号
    int pos=0;//分配给新文件的目录项在数据块中的位置
    for(int i=0;i<cur_inode.i_blocks;i++)
    {
        reload_dir_i(i,&tmp_dir,0,cur_inode);
        int offset=0;//读出的目录项的实际长度累加
        while(tmp_dir.rec_len!=0)//若读出的目录项长度为0则结束循环
        {
            if(tmp_dir.rec_len<0)printf("wrong len");
            if(tmp_dir.inode)
            {
                if(strcmp(tmp_dir.name,name)==0)
                {
                    if(tmp_dir.file_type==1)printf("There has been a file named %s!\n",name);
                    else if(tmp_dir.file_type==2)printf("There has been a directory named %s!\n",name);
                    fclose(fp);
                    return ;
                }
            }
            else//索引结点号为0
            {
                if(tmp_dir.rec_len>=new_len)
                {
                    flag=1;
                    dir=tmp_dir;
                    pos=offset;
                    num=i;
                }
            }
            offset+=tmp_dir.rec_len;
            if(512-offset<7)break;//此数据块已读完(目录项长度至少7字节)
            reload_dir_i(i,&tmp_dir,offset,cur_inode);
        }
        if(tmp_dir.rec_len==0&&flag==0)
        {//从来没有使用过的目录
            if(512-offset>=new_len)
            {//创建新目录项后不会溢出数据块
                flag=1;
                dir=tmp_dir;
                dir.rec_len=7+strlen(name);
                pos=offset;
                num=i;
            }
        }
    }
    if(flag==0)//当前目录的已有数据块不够加入需要创建的目录项
    {//给当前目录分配一个新的数据块并更新当前索引结点的信息
        int block_num=alloc_block();
        if(block_num==-1)   {
            fclose(fp);
            return ;
        }
        num=cur_inode.i_blocks;
        pos=0;
        update_inode_newblock(block_num,&cur_inode);
        update_inode_entry(currentdir_inode,&cur_inode);
        dir.rec_len=7+strlen(name);
    }
    time_t now;
    time(&now);
    dir.file_type=type_num;
    dir.inode=get_inode();
    dir.name_len=strlen(name);
    strcpy(dir.name,name);
    //新建文件/目录的索引结点初始化
    ext2_inode new_inode;
    reload_inode_entry(dir.inode,&new_inode);
    if(type_num==1) new_inode.i_mode =7;// 默认f_r_w_x
    else if(type_num==2) new_inode.i_mode =15;//d_r_w_x
    new_inode.i_blocks=0;
    new_inode.i_size = 0; 
    new_inode.i_atime = now;
    new_inode.i_ctime = now;
    new_inode.i_mtime = now;
    new_inode.i_dtime = 0; // 删除时间为0表示未删除
    new_inode.i_block[0]=alloc_block();//分配数据块并更新数据块位图
    new_inode.i_blocks++;
    if(type_num==2)//目录初始化
        dir_prepare(&new_inode,currentdir_inode,dir.inode);
    update_inode_entry(dir.inode,&new_inode);
    update_dir_i(num,&dir,pos,cur_inode);//在已找到的目录项位置中写入目录项
    cur_inode.i_size += dir.rec_len;
    cur_inode.i_mtime=now;
    update_inode_entry(currentdir_inode,&cur_inode);
    if(type_num==2)//目录初始化
    {
        group_desc.bg_used_dirs_count++;
        update_group_desc();
        printf("Directory %s is created successfully!\n",name);
    }
    else
        printf("File %s is created successfully!\n",name);
    fclose(fp);
}

void delete_file(uint16_t delete_inode_num)
{
    delete_inode_allblock(delete_inode_num);
    free_inode(delete_inode_num);
}
void delete_dir(uint16_t delete_inode_num)
{
    ext2_inode delete_inode;
    reload_inode_entry(delete_inode_num,&delete_inode);
    if(delete_inode.i_size==17)
    {//该目录中没有其他目录或文件
        free_block(delete_inode.i_block[0]);
        free_inode(delete_inode_num);
        group_desc.bg_used_dirs_count--;
        return;
    }
    else{
        ext2_dir_entry tmp_dir;
        for(int i=0;i<delete_inode.i_blocks;i++)
        {
            int offset=17;//读出的目录项的实际长度累加
            reload_dir_i(i,&tmp_dir,17,delete_inode);
            while(tmp_dir.rec_len!=0)//若读出的目录项长度为0则结束循环
            {
                if(tmp_dir.rec_len<0)printf("wrong len\n");
                if(tmp_dir.inode)
                {
                    tmp_dir.inode=0;//其实也可以不用置零，因为之后整个数据块被删除时会全部清零
                    if(tmp_dir.file_type==2)
                    {
                        delete_dir(tmp_dir.inode);
                    }   
                    else if(tmp_dir.file_type==1)
                    {
                        delete_file(tmp_dir.inode);
                    }
                    group_desc.bg_used_dirs_count--;
                }
                offset+=tmp_dir.rec_len;
                if(512-offset<7)break;//此数据块已读完(目录项长度至少7字节)
                reload_dir_i(i,&tmp_dir,offset,delete_inode);
            }
        }
        delete_inode_allblock(delete_inode_num);
        free_inode(delete_inode_num);
    }
    update_group_desc();
}

void delete(uint8_t type_num,char name[255],int flag)
{//注意在search_file中修改当前目录的索引节点大小和修改时间
//flag用于判断是否需要递归删除,删除文件时的flag为0便于search_file中修改索引结点
    fp=fopen(PATH,"rb+");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);
    if((!(cur_inode.i_mode&1))||(!(cur_inode.i_mode&2))) //检查访问权限
    {
        printf("No permission to delete file/directory in current directory!\n");
        return;
    }
    //如果找到了直接在调用的函数中将目录项的索引节点号赋值为0,即删除目录项
    uint16_t delete_inode_num=search_filename(name,type_num,&cur_inode,1+flag);
    
    if(delete_inode_num)
    {
        ext2_inode delete_inode;
        if(type_num==2)//目录
        {
            if(flag==0)//递归删除目录
                delete_dir(delete_inode_num);
            else{//只删除空目录
                reload_inode_entry(delete_inode_num,&delete_inode);
                if(delete_inode.i_size==17)
                {
                    free_block(delete_inode.i_block[0]);
                    free_inode(delete_inode_num);
                }
                else
                {
                    printf("The block is not empty.\n");
                    fclose(fp);
                    return;
                }
            }
        }
        else if(type_num==1)
        {
            delete_file(delete_inode_num);
        }
        time_t now;
        time(&now);
        cur_inode.i_mtime=now;
        update_inode_entry(currentdir_inode,&cur_inode);
    }
    else
    {
        fclose(fp);
        return;
    }
    printf("%s is deleted successfully!\n",name);
    fclose(fp);
}

/*----------   文件   ----------*/
void open_file(unsigned char name[255])
{
    fp=fopen(PATH,"rb");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);//获取当前目录的索引结点
    uint16_t inode_num=search_filename(name,1,&cur_inode,0);
    if(inode_num)
    {
        if(test_fd(inode_num))
            printf("File %s has been opened!\n",name);
        else
        {
            int pos;
            for(pos=0;pos<16;pos++)
            {
                if(fopen_table[pos]==0)
                {
                    fopen_table[pos]=inode_num;
                    printf("File %s is opened successfully!\n",name);
                    return;
                }
            }
            if(pos==16) printf("Fopen_table is full!\n");
        }
    }
}
void close_file(unsigned char name[255])
{
    fp=fopen(PATH,"rb");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);//获取当前目录的索引结点
    uint16_t inode_num=search_filename(name,1,&cur_inode,0);
    if(inode_num)
    {
        if(test_fd(inode_num))
        {
            int pos;
            for(pos=0;pos<16;pos++)
            {
                if(fopen_table[pos]==inode_num)
                {
                    fopen_table[pos]=0;
                    printf("File %s is closed successfully!\n",name);
                }
            }
        }
        else printf("File %s has not been opened!\n",name);
    }
    fclose(fp);
}

void read_file(char name[255])
{
    fp=fopen(PATH,"rb+");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);//获取当前目录的索引结点
    uint16_t inode_num=search_filename(name,1,&cur_inode,0);
    if(inode_num)
    {
        if(test_fd(inode_num))
        {
            ext2_inode read_inode;
            reload_inode_entry(inode_num,&read_inode);
            if(!(read_inode.i_mode&4))//111b:读,写,执行
            {
                printf("No permission to read file %s !\n",name);
                return;
            }
            int i;
            int a=read_inode.i_blocks;
            printf("\n");
            for(i=0;i<read_inode.i_blocks;i++)
            {
                char tmp_block[512];
                reload_block_i(i,tmp_block,read_inode);
                printf("%s",tmp_block);
            }
            if(read_inode.i_size==0) printf("File %s is empty!\n",name);
            else printf("\n");
            printf("\n");
            time_t now;
            time(&now);
            read_inode.i_atime=now;
            update_inode_entry(inode_num,&read_inode);
        }
        else    printf("File %s has not been opened!\n",name);
    }
    fclose(fp);
}
void write_file(unsigned char name[255],int flag,char source[2560])
{//写入文件覆盖原本的内容或者在文件后面追加内容,用flag区分写入方式（和linux的echo命令类似）
    fp=fopen(PATH,"rb+");
    ext2_inode cur_inode;
    uint32_t size=strlen(source);
    //printf("size%d\n",size);
    reload_inode_entry(currentdir_inode,&cur_inode);//获取当前目录的索引结点
    uint16_t inode_num=search_filename(name,1,&cur_inode,0);
    if(!test_fd(inode_num))
    {
        printf("File %s has not been opened!\n",name);
        fclose(fp);
        return;
    }
    if(inode_num&&test_fd(inode_num))
    {
        ext2_inode write_inode;
        reload_inode_entry(inode_num,&write_inode);
        if(!(write_inode.i_mode&2))//111b:读,写,执行
        {
            printf("No permission to write into file %s !\n",name);
            fclose(fp);
            return;
        }
        char tmp_block[512];//数据块缓冲区
        if(flag==0)//写入文件覆盖原本的内容
        {
            if(size>blocksize*(write_inode.i_blocks+group_desc.bg_free_blocks_count))
            {//判断空闲空间是否足够
                printf("No enough space!\n");
                fclose(fp);
                return;
            }
            delete_inode_allblock(inode_num);
            reload_inode_entry(inode_num,&write_inode);
            while((strlen(source)>512))
            {
                int block_num=alloc_block();
                update_inode_newblock(block_num,&write_inode);
                update_inode_entry(inode_num,&write_inode);
                strncpy(tmp_block, source, 512);
                source = source + 512;
                update_block_entry(block_num,tmp_block);
            }
            //需要分配的空间<=512时分配最后一块
            int block_num=alloc_block();
            update_inode_newblock(block_num,&write_inode);
            update_inode_entry(inode_num,&write_inode);
            strcpy(tmp_block, source);
            update_block_entry(block_num,tmp_block);
            write_inode.i_size=size;
        }
        else if(flag==1)//在文件后面追加内容
        {
            if(size>write_inode.i_blocks*blocksize-write_inode.i_size+blocksize*group_desc.bg_free_blocks_count)
            {//可使用空间为文件原有剩余空间+可分配空间
                printf("No enough space!\n");
                fclose(fp);
                return;
            }
            if(size<=write_inode.i_blocks*blocksize-write_inode.i_size)
            {//无需额外分配数据块，直接写入文件最后一块
                reload_block_entry(write_inode.i_block[write_inode.i_blocks-1],tmp_block);
                strcat(tmp_block,source);
                update_block_entry(write_inode.i_block[write_inode.i_blocks-1],tmp_block);
            }
            else
            {//先填满文件最后一块，再分配新的空间
                reload_block_entry(write_inode.i_block[write_inode.i_blocks-1],tmp_block);
                strncat(tmp_block,source,write_inode.i_blocks*blocksize-write_inode.i_size);
                update_block_entry(write_inode.i_block[write_inode.i_blocks-1],tmp_block);
                source=source+write_inode.i_blocks*blocksize-write_inode.i_size;
                while((strlen(source)>512))
                {
                    int block_num=alloc_block();
                    update_inode_newblock(block_num,&write_inode);
                    update_inode_entry(inode_num,&write_inode);
                    strncpy(tmp_block, source, 512);
                    source = source + 512;
                    update_block_entry(block_num,tmp_block);
                }
                //需要分配的空间<=512时分配最后一块
                int block_num=alloc_block();
                update_inode_newblock(block_num,&write_inode);
                update_inode_entry(inode_num,&write_inode);
                strcpy(tmp_block, source);
                update_block_entry(block_num,tmp_block);
            }
            write_inode.i_size+=size;
        }
        time_t now;
        time(&now);
        write_inode.i_mtime=now;
        update_inode_entry(inode_num,&write_inode);
    }
    fclose(fp);
}

void chmod(unsigned char name[255],char rwx[6])
{//修改当前目录下某个文件或目录的访问权限
    fp=fopen(PATH,"rb+");
    ext2_inode cur_inode;
    reload_inode_entry(currentdir_inode,&cur_inode);
    uint16_t inode_num=search_filename(name,0,&cur_inode,0);
    if(!inode_num) return;

    char passw[16];
    printf("Please input the password: ");
    scanf("%s",passw);
    if(strcmp(passw,group_desc.psw))
    {
        printf("Wrong password!\n");
        return;
    }

    ext2_inode ch_inode;
    reload_inode_entry(inode_num,&ch_inode);
    ch_inode.i_mode=0;
    if(strchr(rwx, 'r') != NULL) ch_inode.i_mode+=4;
    if(strchr(rwx, 'w') != NULL) ch_inode.i_mode+=2;
    if(strchr(rwx, 'x') != NULL) ch_inode.i_mode+=1;
    time_t now;
    time(&now);
    ch_inode.i_mtime=now;
    update_inode_entry(inode_num,&ch_inode);
    
    fclose(fp);
}

void help()
{
    printf("1.change dir       : cd $path/to/dir$                  10.create file: mkf $file_name$\n");
    printf("2.change imode     : chmod $name$ $rwx$                11.delete file: rm $file_name$ \n");
    printf("3.create dir       : mkdir $dir_name$                  12.login      : login          \n");
    printf("4.delete empty dir : rmdir $dir_name$                  13.logout     : logout         \n");
    printf("5.delete all in dir: rmdir -r $dir_name$               14.password   : passwd         \n");
    printf("6.open file        : open $file_name$                  15.quit       : quit           \n");
    printf("7.close file       : close $file_name$                 16.list items : ls             \n");
    printf("8.write file       : write $file_name$ >/>> $content$  17.format disk: format         \n");
    printf("9.read file        : read $file_name$                  18.help       : help           \n");
}

/******************************    用户接口层    ****************************************/
void format()
{//格式化，会清空磁盘数据
    char sign[3];
    while(1)
    {
        printf("Sure to format the file system? [Y/n]: ");
        scanf("%s",sign);
        if(sign[0]=='N'||sign[0]=='n') break;
        else if(sign[0]=='Y'||sign[0]=='y')
        {
            initialize_disk();
            initialize_memory();
            break;
        }
        else printf("Please input [Y/n]!\n");
    }
}
void quit()
{
    printf("Thank you for using! Bye~\n");
}
int password()
{//修改密码，修改成功返回1
    char passw[16];
    printf("current password: ");
    scanf("%s",passw);
    if(strcmp(passw,group_desc.psw))
    {
        printf("Wrong password!\n");
        return 0;
    }
    printf("new password: ");
    scanf("%s",passw);
    strcpy(group_desc.psw,passw);
    fp=fopen(PATH,"rb+");
    update_group_desc();
    reload_group_desc();
    fclose(fp);
    return 1;
}
int login()
{
    char passw[16];
    fp=fopen(PATH,"rb");
    reload_group_desc();
    fclose(fp);
    printf("password: ");
    scanf("%s",passw);
    
    if(!strcmp(group_desc.psw,passw))    return 1;
    else{
        printf("Wrong password!");
        scanf("%s",passw);
        if(!strcmp(group_desc.psw,passw))    return 1;
        else quit();
    }
    return 0;
}
void logout()
{
    char sign[3];
    while(1)
    {
        printf("Sure to log out? [Y/n]: ");
        scanf("%s",sign);
        if(sign[0]=='N'||sign[0]=='n') break;
        else if(sign[0]=='Y'||sign[0]=='y')
        {
            initialize_memory();
            printf("[$$$]# ");
            char command_[6];
            scanf("%s",command_);
            if(!strcmp(command_,"login"))
            {
                if(login()) break;
                else return;
            }
            else if(!strcmp(command_,"quit"))
            {
                quit();
                return;
            }
                
        }
        else printf("Please input [Y/n]!\n");
    }
}

void shell()
{
    char command[10];
    ext2_inode tmp_inode;
    int a = 0;
    while (a)
    {
        /* This file defines standard ELF types, structures, and macros.
   Copyright (C) 1995-2022 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef _ELF_H
#define _ELF_H 1

/* Standard ELF types.  */

#include <stdint.h>

/* Type for a 16-bit quantity.  */
typedef uint16_t Elf32_Half;
typedef uint16_t Elf64_Half;

/* Types for signed and unsigned 32-bit quantities.  */
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;

/* Types for signed and unsigned 64-bit quantities.  */
typedef uint64_t Elf32_Xword;
typedef int64_t  Elf32_Sxword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

/* Type of addresses.  */
typedef uint32_t Elf32_Addr;
typedef uint64_t Elf64_Addr;

/* Type of file offsets.  */
typedef uint32_t Elf32_Off;
typedef uint64_t Elf64_Off;

/* Type for section indices, which are 16-bit quantities.  */
typedef uint16_t Elf32_Section;
typedef uint16_t Elf64_Section;

/* Type for version symbol information.  */
typedef Elf32_Half Elf32_Versym;
typedef Elf64_Half Elf64_Versym;


/* The ELF file header.  This appears at the start of every ELF file.  */

#define EI_NIDENT (16)

typedef struct
{
  unsigned char e_ident[EI_NIDENT];     /* Magic number and other info */
  Elf32_Half    e_type;                 /* Object file type */
  Elf32_Half    e_machine;              /* Architecture */
  Elf32_Word    e_version;              /* Object file version */
  Elf32_Addr    e_entry;                /* Entry point virtual address */
  Elf32_Off     e_phoff;                /* Program header table file offset */
  Elf32_Off     e_shoff;                /* Section header table file offset */
  Elf32_Word    e_flags;                /* Processor-specific flags */
  Elf32_Half    e_ehsize;               /* ELF header size in bytes */
  Elf32_Half    e_phentsize;            /* Program header table entry size */
  Elf32_Half    e_phnum;                /* Program header table entry count */
  Elf32_Half    e_shentsize;            /* Section header table entry size */
  Elf32_Half    e_shnum;                /* Section header table entry count */
  Elf32_Half    e_shstrndx;             /* Section header string table index */
} Elf32_Ehdr;

typedef struct
{
  unsigned char e_ident[EI_NIDENT];     /* Magic number and other info */
  Elf64_Half    e_type;                 /* Object file type */
  Elf64_Half    e_machine;              /* Architecture */
  Elf64_Word    e_version;              /* Object file version */
  Elf64_Addr    e_entry;                /* Entry point virtual address */
  Elf64_Off     e_phoff;                /* Program header table file offset */
  Elf64_Off     e_shoff;                /* Section header table file offset */
  Elf64_Word    e_flags;                /* Processor-specific flags */
  Elf64_Half    e_ehsize;               /* ELF header size in bytes */
  Elf64_Half    e_phentsize;            /* Program header table entry size */
  Elf64_Half    e_phnum;                /* Program header table entry count */
  Elf64_Half    e_shentsize;            /* Section header table entry size */
  Elf64_Half    e_shnum;                /* Section header table entry count */
  Elf64_Half    e_shstrndx;             /* Section header string table index */
} Elf64_Ehdr;

/* Fields in the e_ident array.  The EI_* macros are indices into the
   array.  The macros under each EI_* macro are the values the byte
   may have.  */

#define EI_MAG0         0               /* File identification byte 0 index */
#define ELFMAG0         0x7f            /* Magic number byte 0 */

#define EI_MAG1         1               /* File identification byte 1 index */
#define ELFMAG1         'E'             /* Magic number byte 1 */

#define EI_MAG2         2               /* File identification byte 2 index */
#define ELFMAG2         'L'             /* Magic number byte 2 */

#define EI_MAG3         3               /* File identification byte 3 index */
#define ELFMAG3         'F'             /* Magic number byte 3 */

/* Conglomeration of the identification bytes, for easy testing as a word.  */
#define ELFMAG          "\177ELF"
#define SELFMAG         4

#define EI_CLASS        4               /* File class byte index */
#define ELFCLASSNONE    0               /* Invalid class */
#define ELFCLASS32      1               /* 32-bit objects */
#define ELFCLASS64      2               /* 64-bit objects */
#define ELFCLASSNUM     3

#define EI_DATA         5               /* Data encoding byte index */
#define ELFDATANONE     0               /* Invalid data encoding */
#define ELFDATA2LSB     1               /* 2's complement, little endian */
#define ELFDATA2MSB     2               /* 2's complement, big endian */
#define ELFDATANUM      3

#define EI_VERSION      6               /* File version byte index */
                                        /* Value must be EV_CURRENT */

#define EI_OSABI        7               /* OS ABI identification */
#define ELFOSABI_NONE           0       /* UNIX System V ABI */
#define ELFOSABI_SYSV           0       /* Alias.  */
#define ELFOSABI_HPUX           1       /* HP-UX */
#define ELFOSABI_NETBSD         2       /* NetBSD.  */
#define ELFOSABI_GNU            3       /* Object uses GNU ELF extensions.  */
#define ELFOSABI_LINUX          ELFOSABI_GNU /* Compatibility alias.  */
#define ELFOSABI_SOLARIS        6       /* Sun Solaris.  */
#define ELFOSABI_AIX            7       /* IBM AIX.  */
#define ELFOSABI_IRIX           8       /* SGI Irix.  */
#define ELFOSABI_FREEBSD        9       /* FreeBSD.  */
#define ELFOSABI_TRU64          10      /* Compaq TRU64 UNIX.  */
#define ELFOSABI_MODESTO        11      /* Novell Modesto.  */
#define ELFOSABI_OPENBSD        12      /* OpenBSD.  */
#define ELFOSABI_ARM_AEABI      64      /* ARM EABI */
#define ELFOSABI_ARM            97      /* ARM */
#define ELFOSABI_STANDALONE     255     /* Standalone (embedded) application */

#define EI_ABIVERSION   8               /* ABI version */

#define EI_PAD          9               /* Byte index of padding bytes */

/* Legal values for e_type (object file type).  */

#define ET_NONE         0               /* No file type */
#define ET_REL          1               /* Relocatable file */
#define ET_EXEC         2               /* Executable file */
#define ET_DYN          3               /* Shared object file */
#define ET_CORE         4               /* Core file */
#define ET_NUM          5               /* Number of defined types */
#define ET_LOOS         0xfe00          /* OS-specific range start */
#define ET_HIOS         0xfeff          /* OS-specific range end */
#define ET_LOPROC       0xff00          /* Processor-specific range start */
#define ET_HIPROC       0xffff          /* Processor-specific range end */

/* Legal values for e_machine (architecture).  */

#define EM_NONE          0      /* No machine */
#define EM_M32           1      /* AT&T WE 32100 */
#define EM_SPARC         2      /* SUN SPARC */
#define EM_386           3      /* Intel 80386 */
#define EM_68K           4      /* Motorola m68k family */
#define EM_88K           5      /* Motorola m88k family */
#define EM_IAMCU         6      /* Intel MCU */
#define EM_860           7      /* Intel 80860 */
#define EM_MIPS          8      /* MIPS R3000 big-endian */
#define EM_S370          9      /* IBM System/370 */
#define EM_MIPS_RS3_LE  10      /* MIPS R3000 little-endian */
                                /* reserved 11-14 */
#define EM_PARISC       15      /* HPPA */
                                /* reserved 16 */
#define EM_VPP500       17      /* Fujitsu VPP500 */
#define EM_SPARC32PLUS  18      /* Sun's "v8plus" */
#define EM_960          19      /* Intel 80960 */
#define EM_PPC          20      /* PowerPC */
#define EM_PPC64        21      /* PowerPC 64-bit */
#define EM_S390         22      /* IBM S390 */
#define EM_SPU          23      /* IBM SPU/SPC */
                                /* reserved 24-35 */
#define EM_V800         36      /* NEC V800 series */
#define EM_FR20         37      /* Fujitsu FR20 */
#define EM_RH32         38      /* TRW RH-32 */
#define EM_RCE          39      /* Motorola RCE */
#define EM_ARM          40      /* ARM */
#define EM_FAKE_ALPHA   41      /* Digital Alpha */
#define EM_SH           42      /* Hitachi SH */
#define EM_SPARCV9      43      /* SPARC v9 64-bit */
#define EM_TRICORE      44      /* Siemens Tricore */
#define EM_ARC          45      /* Argonaut RISC Core */
#define EM_H8_300       46      /* Hitachi H8/300 */
#define EM_H8_300H      47      /* Hitachi H8/300H */
#define EM_H8S          48      /* Hitachi H8S */
#define EM_H8_500       49      /* Hitachi H8/500 */
#define EM_IA_64        50      /* Intel Merced */
#define EM_MIPS_X       51      /* Stanford MIPS-X */
#define EM_COLDFIRE     52      /* Motorola Coldfire */
#define EM_68HC12       53      /* Motorola M68HC12 */
#define EM_MMA          54      /* Fujitsu MMA Multimedia Accelerator */
#define EM_PCP          55      /* Siemens PCP */
#define EM_NCPU         56      /* Sony nCPU embeeded RISC */
#define EM_NDR1         57      /* Denso NDR1 microprocessor */
#define EM_STARCORE     58      /* Motorola Start*Core processor */
#define EM_ME16         59      /* Toyota ME16 processor */
#define EM_ST100        60      /* STMicroelectronic ST100 processor */
#define EM_TINYJ        61      /* Advanced Logic Corp. Tinyj emb.fam */
#define EM_X86_64       62      /* AMD x86-64 architecture */
#define EM_PDSP         63      /* Sony DSP Processor */
#define EM_PDP10        64      /* Digital PDP-10 */
#define EM_PDP11        65      /* Digital PDP-11 */
#define EM_FX66         66      /* Siemens FX66 microcontroller */
#define EM_ST9PLUS      67      /* STMicroelectronics ST9+ 8/16 mc */
#define EM_ST7          68      /* STmicroelectronics ST7 8 bit mc */
#define EM_68HC16       69      /* Motorola MC68HC16 microcontroller */
#define EM_68HC11       70      /* Motorola MC68HC11 microcontroller */
#define EM_68HC08       71      /* Motorola MC68HC08 microcontroller */
#define EM_68HC05       72      /* Motorola MC68HC05 microcontroller */
#define EM_SVX          73      /* Silicon Graphics SVx */
#define EM_ST19         74      /* STMicroelectronics ST19 8 bit mc */
#define EM_VAX          75      /* Digital VAX */
#define EM_CRIS         76      /* Axis Communications 32-bit emb.proc */
#define EM_JAVELIN      77      /* Infineon Technologies 32-bit emb.proc */
#define EM_FIREPATH     78      /* Element 14 64-bit DSP Processor */
#define EM_ZSP          79      /* LSI Logic 16-bit DSP Processor */
#define EM_MMIX         80      /* Donald Knuth's educational 64-bit proc */
#define EM_HUANY        81      /* Harvard University machine-independent object files */
#define EM_PRISM        82      /* SiTera Prism */
#define EM_AVR          83      /* Atmel AVR 8-bit microcontroller */
#define EM_FR30         84      /* Fujitsu FR30 */
#define EM_D10V         85      /* Mitsubishi D10V */
#define EM_D30V         86      /* Mitsubishi D30V */
#define EM_V850         87      /* NEC v850 */
#define EM_M32R         88      /* Mitsubishi M32R */
#define EM_MN10300      89      /* Matsushita MN10300 */
#define EM_MN10200      90      /* Matsushita MN10200 */
#define EM_PJ           91      /* picoJava */
#define EM_OPENRISC     92      /* OpenRISC 32-bit embedded processor */
#define EM_ARC_COMPACT  93      /* ARC International ARCompact */
#define EM_XTENSA       94      /* Tensilica Xtensa Architecture */
#define EM_VIDEOCORE    95      /* Alphamosaic VideoCore */
#define EM_TMM_GPP      96      /* Thompson Multimedia General Purpose Proc */
#define EM_NS32K        97      /* National Semi. 32000 */
#define EM_TPC          98      /* Tenor Network TPC */
#define EM_SNP1K        99      /* Trebia SNP 1000 */
#define EM_ST200        100     /* STMicroelectronics ST200 */
#define EM_IP2K         101     /* Ubicom IP2xxx */
#define EM_MAX          102     /* MAX processor */
#define EM_CR           103     /* National Semi. CompactRISC */
#define EM_F2MC16       104     /* Fujitsu F2MC16 */
#define EM_MSP430       105     /* Texas Instruments msp430 */
#define EM_BLACKFIN     106     /* Analog Devices Blackfin DSP */
#define EM_SE_C33       107     /* Seiko Epson S1C33 family */
#define EM_SEP          108     /* Sharp embedded microprocessor */
#define EM_ARCA         109     /* Arca RISC */
#define EM_UNICORE      110     /* PKU-Unity & MPRC Peking Uni. mc series */
#define EM_EXCESS       111     /* eXcess configurable cpu */
#define EM_DXP          112     /* Icera Semi. Deep Execution Processor */
#define EM_ALTERA_NIOS2 113     /* Altera Nios II */
#define EM_CRX          114     /* National Semi. CompactRISC CRX */
#define EM_XGATE        115     /* Motorola XGATE */
#define EM_C166         116     /* Infineon C16x/XC16x */
#define EM_M16C         117     /* Renesas M16C */
#define EM_DSPIC30F     118     /* Microchip Technology dsPIC30F */
#define EM_CE           119     /* Freescale Communication Engine RISC */
#define EM_M32C         120     /* Renesas M32C */
                                /* reserved 121-130 */
#define EM_TSK3000      131     /* Altium TSK3000 */
#define EM_RS08         132     /* Freescale RS08 */
#define EM_SHARC        133     /* Analog Devices SHARC family */
#define EM_ECOG2        134     /* Cyan Technology eCOG2 */
#define EM_SCORE7       135     /* Sunplus S+core7 RISC */
#define EM_DSP24        136     /* New Japan Radio (NJR) 24-bit DSP */
#define EM_VIDEOCORE3   137     /* Broadcom VideoCore III */
#define EM_LATTICEMICO32 138    /* RISC for Lattice FPGA */
#define EM_SE_C17       139     /* Seiko Epson C17 */
#define EM_TI_C6000     140     /* Texas Instruments TMS320C6000 DSP */
#define EM_TI_C2000     141     /* Texas Instruments TMS320C2000 DSP */
#define EM_TI_C5500     142     /* Texas Instruments TMS320C55x DSP */
#define EM_TI_ARP32     143     /* Texas Instruments App. Specific RISC */
#define EM_TI_PRU       144     /* Texas Instruments Prog. Realtime Unit */
                                /* reserved 145-159 */
#define EM_MMDSP_PLUS   160     /* STMicroelectronics 64bit VLIW DSP */
#define EM_CYPRESS_M8C  161     /* Cypress M8C */
#define EM_R32C         162     /* Renesas R32C */
#define EM_TRIMEDIA     163     /* NXP Semi. TriMedia */
#define EM_QDSP6        164     /* QUALCOMM DSP6 */
#define EM_8051         165     /* Intel 8051 and variants */
#define EM_STXP7X       166     /* STMicroelectronics STxP7x */
#define EM_NDS32        167     /* Andes Tech. compact code emb. RISC */
#define EM_ECOG1X       168     /* Cyan Technology eCOG1X */
#define EM_MAXQ30       169     /* Dallas Semi. MAXQ30 mc */
#define EM_XIMO16       170     /* New Japan Radio (NJR) 16-bit DSP */
#define EM_MANIK        171     /* M2000 Reconfigurable RISC */
#define EM_CRAYNV2      172     /* Cray NV2 vector architecture */
#define EM_RX           173     /* Renesas RX */
#define EM_METAG        174     /* Imagination Tech. META */
#define EM_MCST_ELBRUS  175     /* MCST Elbrus */
#define EM_ECOG16       176     /* Cyan Technology eCOG16 */
#define EM_CR16         177     /* National Semi. CompactRISC CR16 */
#define EM_ETPU         178     /* Freescale Extended Time Processing Unit */
#define EM_SLE9X        179     /* Infineon Tech. SLE9X */
#define EM_L10M         180     /* Intel L10M */
#define EM_K10M         181     /* Intel K10M */
                                /* reserved 182 */
#define EM_AARCH64      183     /* ARM AARCH64 */
                                /* reserved 184 */
#define EM_AVR32        185     /* Amtel 32-bit microprocessor */
#define EM_STM8         186     /* STMicroelectronics STM8 */
#define EM_TILE64       187     /* Tilera TILE64 */
#define EM_TILEPRO      188     /* Tilera TILEPro */
#define EM_MICROBLAZE   189     /* Xilinx MicroBlaze */
#define EM_CUDA         190     /* NVIDIA CUDA */
#define EM_TILEGX       191     /* Tilera TILE-Gx */
#define EM_CLOUDSHIELD  192     /* CloudShield */
#define EM_COREA_1ST    193     /* KIPO-KAIST Core-A 1st gen. */
#define EM_COREA_2ND    194     /* KIPO-KAIST Core-A 2nd gen. */
#define EM_ARCV2        195     /* Synopsys ARCv2 ISA.  */
#define EM_OPEN8        196     /* Open8 RISC */
#define EM_RL78         197     /* Renesas RL78 */
#define EM_VIDEOCORE5   198     /* Broadcom VideoCore V */
#define EM_78KOR        199     /* Renesas 78KOR */
#define EM_56800EX      200     /* Freescale 56800EX DSC */
#define EM_BA1          201     /* Beyond BA1 */
#define EM_BA2          202     /* Beyond BA2 */
#define EM_XCORE        203     /* XMOS xCORE */
#define EM_MCHP_PIC     204     /* Microchip 8-bit PIC(r) */
#define EM_INTELGT      205     /* Intel Graphics Technology */
                                /* reserved 206-209 */
#define EM_KM32         210     /* KM211 KM32 */
#define EM_KMX32        211     /* KM211 KMX32 */
#define EM_EMX16        212     /* KM211 KMX16 */
#define EM_EMX8         213     /* KM211 KMX8 */
#define EM_KVARC        214     /* KM211 KVARC */
#define EM_CDP          215     /* Paneve CDP */
#define EM_COGE         216     /* Cognitive Smart Memory Processor */
#define EM_COOL         217     /* Bluechip CoolEngine */
#define EM_NORC         218     /* Nanoradio Optimized RISC */
#define EM_CSR_KALIMBA  219     /* CSR Kalimba */
#define EM_Z80          220     /* Zilog Z80 */
#define EM_VISIUM       221     /* Controls and Data Services VISIUMcore */
#define EM_FT32         222     /* FTDI Chip FT32 */
#define EM_MOXIE        223     /* Moxie processor */
#define EM_AMDGPU       224     /* AMD GPU */
                                /* reserved 225-242 */
#define EM_RISCV        243     /* RISC-V */

#define EM_BPF          247     /* Linux BPF -- in-kernel virtual machine */
#define EM_CSKY         252     /* C-SKY */
#define EM_LOONGARCH    258     /* LoongArch */

#define EM_NUM          259

/* Old spellings/synonyms.  */

#define EM_ARC_A5       EM_ARC_COMPACT

/* If it is necessary to assign new unofficial EM_* values, please
   pick large random numbers (0x8523, 0xa7f2, etc.) to minimize the
   chances of collision with official or non-GNU unofficial values.  */

#define EM_ALPHA        0x9026

/* Legal values for e_version (version).  */

#define EV_NONE         0               /* Invalid ELF version */
#define EV_CURRENT      1               /* Current version */
#define EV_NUM          2

/* Section header.  */

typedef struct
{
  Elf32_Word    sh_name;                /* Section name (string tbl index) */
  Elf32_Word    sh_type;                /* Section type */
  Elf32_Word    sh_flags;               /* Section flags */
  Elf32_Addr    sh_addr;                /* Section virtual addr at execution */
  Elf32_Off     sh_offset;              /* Section file offset */
  Elf32_Word    sh_size;                /* Section size in bytes */
  Elf32_Word    sh_link;                /* Link to another section */
  Elf32_Word    sh_info;                /* Additional section information */
  Elf32_Word    sh_addralign;           /* Section alignment */
  Elf32_Word    sh_entsize;             /* Entry size if section holds table */
} Elf32_Shdr;

typedef struct
{
  Elf64_Word    sh_name;                /* Section name (string tbl index) */
  Elf64_Word    sh_type;                /* Section type */
  Elf64_Xword   sh_flags;               /* Section flags */
  Elf64_Addr    sh_addr;                /* Section virtual addr at execution */
  Elf64_Off     sh_offset;              /* Section file offset */
  Elf64_Xword   sh_size;                /* Section size in bytes */
  Elf64_Word    sh_link;                /* Link to another section */
  Elf64_Word    sh_info;                /* Additional section information */
  Elf64_Xword   sh_addralign;           /* Section alignment */
  Elf64_Xword   sh_entsize;             /* Entry size if section holds table */
} Elf64_Shdr;

/* Special section indices.  */

#define SHN_UNDEF       0               /* Undefined section */
#define SHN_LORESERVE   0xff00          /* Start of reserved indices */
#define SHN_LOPROC      0xff00          /* Start of processor-specific */
#define SHN_BEFORE      0xff00          /* Order section before all others
                                           (Solaris).  */
#define SHN_AFTER       0xff01          /* Order section after all others
                                           (Solaris).  */
#define SHN_HIPROC      0xff1f          /* End of processor-specific */
#define SHN_LOOS        0xff20          /* Start of OS-specific */
#define SHN_HIOS        0xff3f          /* End of OS-specific */
#define SHN_ABS         0xfff1          /* Associated symbol is absolute */
#define SHN_COMMON      0xfff2          /* Associated symbol is common */
#define SHN_XINDEX      0xffff          /* Index is in extra table.  */
#define SHN_HIRESERVE   0xffff          /* End of reserved indices */

/* Legal values for sh_type (section type).  */

#define SHT_NULL          0             /* Section header table entry unused */
#define SHT_PROGBITS      1             /* Program data */
#define SHT_SYMTAB        2             /* Symbol table */
#define SHT_STRTAB        3             /* String table */
#define SHT_RELA          4             /* Relocation entries with addends */
#define SHT_HASH          5             /* Symbol hash table */
#define SHT_DYNAMIC       6             /* Dynamic linking information */
#define SHT_NOTE          7             /* Notes */
#define SHT_NOBITS        8             /* Program space with no data (bss) */
#define SHT_REL           9             /* Relocation entries, no addends */
#define SHT_SHLIB         10            /* Reserved */
#define SHT_DYNSYM        11            /* Dynamic linker symbol table */
#define SHT_INIT_ARRAY    14            /* Array of constructors */
#define SHT_FINI_ARRAY    15            /* Array of destructors */
#define SHT_PREINIT_ARRAY 16            /* Array of pre-constructors */
#define SHT_GROUP         17            /* Section group */
#define SHT_SYMTAB_SHNDX  18            /* Extended section indices */
#define SHT_RELR          19            /* RELR relative relocations */
#define SHT_NUM           20            /* Number of defined types.  */
#define SHT_LOOS          0x60000000    /* Start OS-specific.  */
#define SHT_GNU_ATTRIBUTES 0x6ffffff5   /* Object attributes.  */
#define SHT_GNU_HASH      0x6ffffff6    /* GNU-style hash table.  */
#define SHT_GNU_LIBLIST   0x6ffffff7    /* Prelink library list */
#define SHT_CHECKSUM      0x6ffffff8    /* Checksum for DSO content.  */
#define SHT_LOSUNW        0x6ffffffa    /* Sun-specific low bound.  */
#define SHT_SUNW_move     0x6ffffffa
#define SHT_SUNW_COMDAT   0x6ffffffb
#define SHT_SUNW_syminfo  0x6ffffffc
#define SHT_GNU_verdef    0x6ffffffd    /* Version definition section.  */
#define SHT_GNU_verneed   0x6ffffffe    /* Version needs section.  */
#define SHT_GNU_versym    0x6fffffff    /* Version symbol table.  */
#define SHT_HISUNW        0x6fffffff    /* Sun-specific high bound.  */
#define SHT_HIOS          0x6fffffff    /* End OS-specific type */
#define SHT_LOPROC        0x70000000    /* Start of processor-specific */
#define SHT_HIPROC        0x7fffffff    /* End of processor-specific */
#define SHT_LOUSER        0x80000000    /* Start of application-specific */
#define SHT_HIUSER        0x8fffffff    /* End of application-specific */

/* Legal values for sh_flags (section flags).  */

#define SHF_WRITE            (1 << 0)   /* Writable */
#define SHF_ALLOC            (1 << 1)   /* Occupies memory during execution */
#define SHF_EXECINSTR        (1 << 2)   /* Executable */
#define SHF_MERGE            (1 << 4)   /* Might be merged */
#define SHF_STRINGS          (1 << 5)   /* Contains nul-terminated strings */
#define SHF_INFO_LINK        (1 << 6)   /* `sh_info' contains SHT index */
#define SHF_LINK_ORDER       (1 << 7)   /* Preserve order after combining */
#define SHF_OS_NONCONFORMING (1 << 8)   /* Non-standard OS specific handling
                                           required */
#define SHF_GROUP            (1 << 9)   /* Section is member of a group.  */
#define SHF_TLS              (1 << 10)  /* Section hold thread-local data.  */
#define SHF_COMPRESSED       (1 << 11)  /* Section with compressed data. */
#define SHF_MASKOS           0x0ff00000 /* OS-specific.  */
#define SHF_MASKPROC         0xf0000000 /* Processor-specific */
#define SHF_GNU_RETAIN       (1 << 21)  /* Not to be GCed by linker.  */
#define SHF_ORDERED          (1 << 30)  /* Special ordering requirement
                                           (Solaris).  */
#define SHF_EXCLUDE          (1U << 31) /* Section is excluded unless
                                           referenced or allocated (Solaris).*/

/* Section compression header.  Used when SHF_COMPRESSED is set.  */

typedef struct
{
  Elf32_Word    ch_type;        /* Compression format.  */
  Elf32_Word    ch_size;        /* Uncompressed data size.  */
  Elf32_Word    ch_addralign;   /* Uncompressed data alignment.  */
} Elf32_Chdr;

typedef struct
{
  Elf64_Word    ch_type;        /* Compression format.  */
  Elf64_Word    ch_reserved;
  Elf64_Xword   ch_size;        /* Uncompressed data size.  */
  Elf64_Xword   ch_addralign;   /* Uncompressed data alignment.  */
} Elf64_Chdr;

/* Legal values for ch_type (compression algorithm).  */
#define ELFCOMPRESS_ZLIB        1          /* ZLIB/DEFLATE algorithm.  */
#define ELFCOMPRESS_LOOS        0x60000000 /* Start of OS-specific.  */
#define ELFCOMPRESS_HIOS        0x6fffffff /* End of OS-specific.  */
#define ELFCOMPRESS_LOPROC      0x70000000 /* Start of processor-specific.  */
#define ELFCOMPRESS_HIPROC      0x7fffffff /* End of processor-specific.  */

/* Section group handling.  */
#define GRP_COMDAT      0x1             /* Mark group as COMDAT.  */

/* Symbol table entry.  */

typedef struct
{
  Elf32_Word    st_name;                /* Symbol name (string tbl index) */
  Elf32_Addr    st_value;               /* Symbol value */
  Elf32_Word    st_size;                /* Symbol size */
  unsigned char st_info;                /* Symbol type and binding */
  unsigned char st_other;               /* Symbol visibility */
  Elf32_Section st_shndx;               /* Section index */
} Elf32_Sym;

typedef struct
{
  Elf64_Word    st_name;                /* Symbol name (string tbl index) */
  unsigned char st_info;                /* Symbol type and binding */
  unsigned char st_other;               /* Symbol visibility */
  Elf64_Section st_shndx;               /* Section index */
  Elf64_Addr    st_value;               /* Symbol value */
  Elf64_Xword   st_size;                /* Symbol size */
} Elf64_Sym;

/* The syminfo section if available contains additional information about
   every dynamic symbol.  */

typedef struct
{
  Elf32_Half si_boundto;                /* Direct bindings, symbol bound to */
  Elf32_Half si_flags;                  /* Per symbol flags */
} Elf32_Syminfo;

typedef struct
{
  Elf64_Half si_boundto;                /* Direct bindings, symbol bound to */
  Elf64_Half si_flags;                  /* Per symbol flags */
} Elf64_Syminfo;

/* Possible values for si_boundto.  */
#define SYMINFO_BT_SELF         0xffff  /* Symbol bound to self */
#define SYMINFO_BT_PARENT       0xfffe  /* Symbol bound to parent */
#define SYMINFO_BT_LOWRESERVE   0xff00  /* Beginning of reserved entries */

/* Possible bitmasks for si_flags.  */
#define SYMINFO_FLG_DIRECT      0x0001  /* Direct bound symbol */
#define SYMINFO_FLG_PASSTHRU    0x0002  /* Pass-thru symbol for translator */
#define SYMINFO_FLG_COPY        0x0004  /* Symbol is a copy-reloc */
#define SYMINFO_FLG_LAZYLOAD    0x0008  /* Symbol bound to object to be lazy
                                           loaded */
/* Syminfo version values.  */
#define SYMINFO_NONE            0
#define SYMINFO_CURRENT         1
#define SYMINFO_NUM             2


/* How to extract and insert information held in the st_info field.  */

#define ELF32_ST_BIND(val)              (((unsigned char) (val)) >> 4)
#define ELF32_ST_TYPE(val)              ((val) & 0xf)
#define ELF32_ST_INFO(bind, type)       (((bind) << 4) + ((type) & 0xf))

/* Both Elf32_Sym and Elf64_Sym use the same one-byte st_info field.  */
#define ELF64_ST_BIND(val)              ELF32_ST_BIND (val)
#define ELF64_ST_TYPE(val)              ELF32_ST_TYPE (val)
#define ELF64_ST_INFO(bind, type)       ELF32_ST_INFO ((bind), (type))

/* Legal values for ST_BIND subfield of st_info (symbol binding).  */

#define STB_LOCAL       0               /* Local symbol */
#define STB_GLOBAL      1               /* Global symbol */
#define STB_WEAK        2               /* Weak symbol */
#define STB_NUM         3               /* Number of defined types.  */
#define STB_LOOS        10              /* Start of OS-specific */
#define STB_GNU_UNIQUE  10              /* Unique symbol.  */
#define STB_HIOS        12              /* End of OS-specific */
#define STB_LOPROC      13              /* Start of processor-specific */
#define STB_HIPROC      15              /* End of processor-specific */

/* Legal values for ST_TYPE subfield of st_info (symbol type).  */

#define STT_NOTYPE      0               /* Symbol type is unspecified */
#define STT_OBJECT      1               /* Symbol is a data object */
#define STT_FUNC        2               /* Symbol is a code object */
#define STT_SECTION     3               /* Symbol associated with a section */
#define STT_FILE        4               /* Symbol's name is file name */
#define STT_COMMON      5               /* Symbol is a common data object */
#define STT_TLS         6               /* Symbol is thread-local data object*/
#define STT_NUM         7               /* Number of defined types.  */
#define STT_LOOS        10              /* Start of OS-specific */
#define STT_GNU_IFUNC   10              /* Symbol is indirect code object */
#define STT_HIOS        12              /* End of OS-specific */
#define STT_LOPROC      13              /* Start of processor-specific */
#define STT_HIPROC      15              /* End of processor-specific */


/* Symbol table indices are found in the hash buckets and chain table
   of a symbol hash table section.  This special index value indicates
   the end of a chain, meaning no further symbols are found in that bucket.  */

#define STN_UNDEF       0               /* End of a chain.  */


/* How to extract and insert information held in the st_other field.  */

#define ELF32_ST_VISIBILITY(o)  ((o) & 0x03)

/* For ELF64 the definitions are the same.  */
#define ELF64_ST_VISIBILITY(o)  ELF32_ST_VISIBILITY (o)

/* Symbol visibility specification encoded in the st_other field.  */
#define STV_DEFAULT     0               /* Default symbol visibility rules */
#define STV_INTERNAL    1               /* Processor specific hidden class */
#define STV_HIDDEN      2               /* Sym unavailable in other modules */
#define STV_PROTECTED   3               /* Not preemptible, not exported */


/* Relocation table entry without addend (in section of type SHT_REL).  */

typedef struct
{
  Elf32_Addr    r_offset;               /* Address */
  Elf32_Word    r_info;                 /* Relocation type and symbol index */
} Elf32_Rel;

/* I have seen two different definitions of the Elf64_Rel and
   Elf64_Rela structures, so we'll leave them out until Novell (or
   whoever) gets their act together.  */
/* The following, at least, is used on Sparc v9, MIPS, and Alpha.  */

typedef struct
{
  Elf64_Addr    r_offset;               /* Address */
  Elf64_Xword   r_info;                 /* Relocation type and symbol index */
} Elf64_Rel;

/* Relocation table entry with addend (in section of type SHT_RELA).  */

typedef struct
{
  Elf32_Addr    r_offset;               /* Address */
  Elf32_Word    r_info;                 /* Relocation type and symbol index */
  Elf32_Sword   r_addend;               /* Addend */
} Elf32_Rela;

typedef struct
{
  Elf64_Addr    r_offset;               /* Address */
  Elf64_Xword   r_info;                 /* Relocation type and symbol index */
  Elf64_Sxword  r_addend;               /* Addend */
} Elf64_Rela;

/* RELR relocation table entry */

typedef Elf32_Word      Elf32_Relr;
typedef Elf64_Xword     Elf64_Relr;

/* How to extract and insert information held in the r_info field.  */

#define ELF32_R_SYM(val)                ((val) >> 8)
#define ELF32_R_TYPE(val)               ((val) & 0xff)
#define ELF32_R_INFO(sym, type)         (((sym) << 8) + ((type) & 0xff))

#define ELF64_R_SYM(i)                  ((i) >> 32)
#define ELF64_R_TYPE(i)                 ((i) & 0xffffffff)
#define ELF64_R_INFO(sym,type)          ((((Elf64_Xword) (sym)) << 32) + (type))

/* Program segment header.  */

typedef struct
{
  Elf32_Word    p_type;                 /* Segment type */
  Elf32_Off     p_offset;               /* Segment file offset */
  Elf32_Addr    p_vaddr;                /* Segment virtual address */
  Elf32_Addr    p_paddr;                /* Segment physical address */
  Elf32_Word    p_filesz;               /* Segment size in file */
  Elf32_Word    p_memsz;                /* Segment size in memory */
  Elf32_Word    p_flags;                /* Segment flags */
  Elf32_Word    p_align;                /* Segment alignment */
} Elf32_Phdr;

typedef struct
{
  Elf64_Word    p_type;                 /* Segment type */
  Elf64_Word    p_flags;                /* Segment flags */
  Elf64_Off     p_offset;               /* Segment file offset */
  Elf64_Addr    p_vaddr;                /* Segment virtual address */
  Elf64_Addr    p_paddr;                /* Segment physical address */
  Elf64_Xword   p_filesz;               /* Segment size in file */
  Elf64_Xword   p_memsz;                /* Segment size in memory */
  Elf64_Xword   p_align;                /* Segment alignment */
} Elf64_Phdr;

/* Special value for e_phnum.  This indicates that the real number of
   program headers is too large to fit into e_phnum.  Instead the real
   value is in the field sh_info of section 0.  */

#define PN_XNUM         0xffff

/* Legal values for p_type (segment type).  */

#define PT_NULL         0               /* Program header table entry unused */
#define PT_LOAD         1               /* Loadable program segment */
#define PT_DYNAMIC      2               /* Dynamic linking information */
#define PT_INTERP       3               /* Program interpreter */
#define PT_NOTE         4               /* Auxiliary information */
#define PT_SHLIB        5               /* Reserved */
#define PT_PHDR         6               /* Entry for header table itself */
#define PT_TLS          7               /* Thread-local storage segment */
#define PT_NUM          8               /* Number of defined types */
#define PT_LOOS         0x60000000      /* Start of OS-specific */
#define PT_GNU_EH_FRAME 0x6474e550      /* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK    0x6474e551      /* Indicates stack executability */
#define PT_GNU_RELRO    0x6474e552      /* Read-only after relocation */
#define PT_GNU_PROPERTY 0x6474e553      /* GNU property */
#define PT_LOSUNW       0x6ffffffa
#define PT_SUNWBSS      0x6ffffffa      /* Sun Specific segment */
#define PT_SUNWSTACK    0x6ffffffb      /* Stack segment */
#define PT_HISUNW       0x6fffffff
#define PT_HIOS         0x6fffffff      /* End of OS-specific */
#define PT_LOPROC       0x70000000      /* Start of processor-specific */
#define PT_HIPROC       0x7fffffff      /* End of processor-specific */

/* Legal values for p_flags (segment flags).  */

#define PF_X            (1 << 0)        /* Segment is executable */
#define PF_W            (1 << 1)        /* Segment is writable */
#define PF_R            (1 << 2)        /* Segment is readable */
#define PF_MASKOS       0x0ff00000      /* OS-specific */
#define PF_MASKPROC     0xf0000000      /* Processor-specific */

/* Legal values for note segment descriptor types for core files. */

#define NT_PRSTATUS     1               /* Contains copy of prstatus struct */
#define NT_PRFPREG      2               /* Contains copy of fpregset
                                           struct.  */
#define NT_FPREGSET     2               /* Contains copy of fpregset struct */
#define NT_PRPSINFO     3               /* Contains copy of prpsinfo struct */
#define NT_PRXREG       4               /* Contains copy of prxregset struct */
#define NT_TASKSTRUCT   4               /* Contains copy of task structure */
#define NT_PLATFORM     5               /* String from sysinfo(SI_PLATFORM) */
#define NT_AUXV         6               /* Contains copy of auxv array */
#define NT_GWINDOWS     7               /* Contains copy of gwindows struct */
#define NT_ASRS         8               /* Contains copy of asrset struct */
#define NT_PSTATUS      10              /* Contains copy of pstatus struct */
#define NT_PSINFO       13              /* Contains copy of psinfo struct */
#define NT_PRCRED       14              /* Contains copy of prcred struct */
#define NT_UTSNAME      15              /* Contains copy of utsname struct */
#define NT_LWPSTATUS    16              /* Contains copy of lwpstatus struct */
#define NT_LWPSINFO     17              /* Contains copy of lwpinfo struct */
#define NT_PRFPXREG     20              /* Contains copy of fprxregset struct */
#define NT_SIGINFO      0x53494749      /* Contains copy of siginfo_t,
                                           size might increase */
#define NT_FILE         0x46494c45      /* Contains information about mapped
                                           files */
#define NT_PRXFPREG     0x46e62b7f      /* Contains copy of user_fxsr_struct */
#define NT_PPC_VMX      0x100           /* PowerPC Altivec/VMX registers */
#define NT_PPC_SPE      0x101           /* PowerPC SPE/EVR registers */
#define NT_PPC_VSX      0x102           /* PowerPC VSX registers */
#define NT_PPC_TAR      0x103           /* Target Address Register */
#define NT_PPC_PPR      0x104           /* Program Priority Register */
#define NT_PPC_DSCR     0x105           /* Data Stream Control Register */
#define NT_PPC_EBB      0x106           /* Event Based Branch Registers */
#define NT_PPC_PMU      0x107           /* Performance Monitor Registers */
#define NT_PPC_TM_CGPR  0x108           /* TM checkpointed GPR Registers */
#define NT_PPC_TM_CFPR  0x109           /* TM checkpointed FPR Registers */
#define NT_PPC_TM_CVMX  0x10a           /* TM checkpointed VMX Registers */
#define NT_PPC_TM_CVSX  0x10b           /* TM checkpointed VSX Registers */
#define NT_PPC_TM_SPR   0x10c           /* TM Special Purpose Registers */
#define NT_PPC_TM_CTAR  0x10d           /* TM checkpointed Target Address
                                           Register */
#define NT_PPC_TM_CPPR  0x10e           /* TM checkpointed Program Priority
                                           Register */
#define NT_PPC_TM_CDSCR 0x10f           /* TM checkpointed Data Stream Control
                                           Register */
#define NT_PPC_PKEY     0x110           /* Memory Protection Keys
                                           registers.  */
#define NT_386_TLS      0x200           /* i386 TLS slots (struct user_desc) */
#define NT_386_IOPERM   0x201           /* x86 io permission bitmap (1=deny) */
#define NT_X86_XSTATE   0x202           /* x86 extended state using xsave */
#define NT_S390_HIGH_GPRS       0x300   /* s390 upper register halves */
#define NT_S390_TIMER   0x301           /* s390 timer register */
#define NT_S390_TODCMP  0x302           /* s390 TOD clock comparator register */
#define NT_S390_TODPREG 0x303           /* s390 TOD programmable register */
#define NT_S390_CTRS    0x304           /* s390 control registers */
#define NT_S390_PREFIX  0x305           /* s390 prefix register */
#define NT_S390_LAST_BREAK      0x306   /* s390 breaking event address */
#define NT_S390_SYSTEM_CALL     0x307   /* s390 system call restart data */
#define NT_S390_TDB     0x308           /* s390 transaction diagnostic block */
#define NT_S390_VXRS_LOW        0x309   /* s390 vector registers 0-15
                                           upper half.  */
#define NT_S390_VXRS_HIGH       0x30a   /* s390 vector registers 16-31.  */
#define NT_S390_GS_CB   0x30b           /* s390 guarded storage registers.  */
#define NT_S390_GS_BC   0x30c           /* s390 guarded storage
                                           broadcast control block.  */
#define NT_S390_RI_CB   0x30d           /* s390 runtime instrumentation.  */
#define NT_ARM_VFP      0x400           /* ARM VFP/NEON registers */
#define NT_ARM_TLS      0x401           /* ARM TLS register */
#define NT_ARM_HW_BREAK 0x402           /* ARM hardware breakpoint registers */
#define NT_ARM_HW_WATCH 0x403           /* ARM hardware watchpoint registers */
#define NT_ARM_SYSTEM_CALL      0x404   /* ARM system call number */
#define NT_ARM_SVE      0x405           /* ARM Scalable Vector Extension
                                           registers */
#define NT_ARM_PAC_MASK 0x406           /* ARM pointer authentication
                                           code masks.  */
#define NT_ARM_PACA_KEYS        0x407   /* ARM pointer authentication
                                           address keys.  */
#define NT_ARM_PACG_KEYS        0x408   /* ARM pointer authentication
                                           generic key.  */
#define NT_ARM_TAGGED_ADDR_CTRL 0x409   /* AArch64 tagged address
                                           control.  */
#define NT_ARM_PAC_ENABLED_KEYS 0x40a   /* AArch64 pointer authentication
                                           enabled keys.  */
#define NT_VMCOREDD     0x700           /* Vmcore Device Dump Note.  */
#define NT_MIPS_DSP     0x800           /* MIPS DSP ASE registers.  */
#define NT_MIPS_FP_MODE 0x801           /* MIPS floating-point mode.  */
#define NT_MIPS_MSA     0x802           /* MIPS SIMD registers.  */

/* Legal values for the note segment descriptor types for object files.  */

#define NT_VERSION      1               /* Contains a version string.  */


/* Dynamic section entry.  */

typedef struct
{
  Elf32_Sword   d_tag;                  /* Dynamic entry type */
  union
    {
      Elf32_Word d_val;                 /* Integer value */
      Elf32_Addr d_ptr;                 /* Address value */
    } d_un;
} Elf32_Dyn;

typedef struct
{
  Elf64_Sxword  d_tag;                  /* Dynamic entry type */
  union
    {
      Elf64_Xword d_val;                /* Integer value */
      Elf64_Addr d_ptr;                 /* Address value */
    } d_un;
} Elf64_Dyn;

/* Legal values for d_tag (dynamic entry type).  */

#define DT_NULL         0               /* Marks end of dynamic section */
#define DT_NEEDED       1               /* Name of needed library */
#define DT_PLTRELSZ     2               /* Size in bytes of PLT relocs */
#define DT_PLTGOT       3               /* Processor defined value */
#define DT_HASH         4               /* Address of symbol hash table */
#define DT_STRTAB       5               /* Address of string table */
#define DT_SYMTAB       6               /* Address of symbol table */
#define DT_RELA         7               /* Address of Rela relocs */
#define DT_RELASZ       8               /* Total size of Rela relocs */
#define DT_RELAENT      9               /* Size of one Rela reloc */
#define DT_STRSZ        10              /* Size of string table */
#define DT_SYMENT       11              /* Size of one symbol table entry */
#define DT_INIT         12              /* Address of init function */
#define DT_FINI         13              /* Address of termination function */
#define DT_SONAME       14              /* Name of shared object */
#define DT_RPATH        15              /* Library search path (deprecated) */
#define DT_SYMBOLIC     16              /* Start symbol search here */
#define DT_REL          17              /* Address of Rel relocs */
#define DT_RELSZ        18              /* Total size of Rel relocs */
#define DT_RELENT       19              /* Size of one Rel reloc */
#define DT_PLTREL       20              /* Type of reloc in PLT */
#define DT_DEBUG        21              /* For debugging; unspecified */
#define DT_TEXTREL      22              /* Reloc might modify .text */
#define DT_JMPREL       23              /* Address of PLT relocs */
#define DT_BIND_NOW     24              /* Process relocations of object */
#define DT_INIT_ARRAY   25              /* Array with addresses of init fct */
#define DT_FINI_ARRAY   26              /* Array with addresses of fini fct */
#define DT_INIT_ARRAYSZ 27              /* Size in bytes of DT_INIT_ARRAY */
#define DT_FINI_ARRAYSZ 28              /* Size in bytes of DT_FINI_ARRAY */
#define DT_RUNPATH      29              /* Library search path */
#define DT_FLAGS        30              /* Flags for the object being loaded */
#define DT_ENCODING     32              /* Start of encoded range */
#define DT_PREINIT_ARRAY 32             /* Array with addresses of preinit fct*/
#define DT_PREINIT_ARRAYSZ 33           /* size in bytes of DT_PREINIT_ARRAY */
#define DT_SYMTAB_SHNDX 34              /* Address of SYMTAB_SHNDX section */
#define DT_RELRSZ       35              /* Total size of RELR relative relocations */
#define DT_RELR         36              /* Address of RELR relative relocations */
#define DT_RELRENT      37              /* Size of one RELR relative relocaction */
#define DT_NUM          38              /* Number used */
#define DT_LOOS         0x6000000d      /* Start of OS-specific */
#define DT_HIOS         0x6ffff000      /* End of OS-specific */
#define DT_LOPROC       0x70000000      /* Start of processor-specific */
#define DT_HIPROC       0x7fffffff      /* End of processor-specific */
#define DT_PROCNUM      DT_MIPS_NUM     /* Most used by any processor */

/* DT_* entries which fall between DT_VALRNGHI & DT_VALRNGLO use the
   Dyn.d_un.d_val field of the Elf*_Dyn structure.  This follows Sun's
   approach.  */
#define DT_VALRNGLO     0x6ffffd00
#define DT_GNU_PRELINKED 0x6ffffdf5     /* Prelinking timestamp */
#define DT_GNU_CONFLICTSZ 0x6ffffdf6    /* Size of conflict section */
#define DT_GNU_LIBLISTSZ 0x6ffffdf7     /* Size of library list */
#define DT_CHECKSUM     0x6ffffdf8
#define DT_PLTPADSZ     0x6ffffdf9
#define DT_MOVEENT      0x6ffffdfa
#define DT_MOVESZ       0x6ffffdfb
#define DT_FEATURE_1    0x6ffffdfc      /* Feature selection (DTF_*).  */
#define DT_POSFLAG_1    0x6ffffdfd      /* Flags for DT_* entries, effecting
                                           the following DT_* entry.  */
#define DT_SYMINSZ      0x6ffffdfe      /* Size of syminfo table (in bytes) */
#define DT_SYMINENT     0x6ffffdff      /* Entry size of syminfo */
#define DT_VALRNGHI     0x6ffffdff
#define DT_VALTAGIDX(tag)       (DT_VALRNGHI - (tag))   /* Reverse order! */
#define DT_VALNUM 12

/* DT_* entries which fall between DT_ADDRRNGHI & DT_ADDRRNGLO use the
   Dyn.d_un.d_ptr field of the Elf*_Dyn structure.

   If any adjustment is made to the ELF object after it has been
   built these entries will need to be adjusted.  */
#define DT_ADDRRNGLO    0x6ffffe00
#define DT_GNU_HASH     0x6ffffef5      /* GNU-style hash table.  */
#define DT_TLSDESC_PLT  0x6ffffef6
#define DT_TLSDESC_GOT  0x6ffffef7
#define DT_GNU_CONFLICT 0x6ffffef8      /* Start of conflict section */
#define DT_GNU_LIBLIST  0x6ffffef9      /* Library list */
#define DT_CONFIG       0x6ffffefa      /* Configuration information.  */
#define DT_DEPAUDIT     0x6ffffefb      /* Dependency auditing.  */
#define DT_AUDIT        0x6ffffefc      /* Object auditing.  */
#define DT_PLTPAD       0x6ffffefd      /* PLT padding.  */
#define DT_MOVETAB      0x6ffffefe      /* Move table.  */
#define DT_SYMINFO      0x6ffffeff      /* Syminfo table.  */
#define DT_ADDRRNGHI    0x6ffffeff
#define DT_ADDRTAGIDX(tag)      (DT_ADDRRNGHI - (tag))  /* Reverse order! */
#define DT_ADDRNUM 11

/* The versioning entry types.  The next are defined as part of the
   GNU extension.  */
#define DT_VERSYM       0x6ffffff0

#define DT_RELACOUNT    0x6ffffff9
#define DT_RELCOUNT     0x6ffffffa

/* These were chosen by Sun.  */
#define DT_FLAGS_1      0x6ffffffb      /* State flags, see DF_1_* below.  */
#define DT_VERDEF       0x6ffffffc      /* Address of version definition
                                           table */
#define DT_VERDEFNUM    0x6ffffffd      /* Number of version definitions */
#define DT_VERNEED      0x6ffffffe      /* Address of table with needed
                                           versions */
#define DT_VERNEEDNUM   0x6fffffff      /* Number of needed versions */
#define DT_VERSIONTAGIDX(tag)   (DT_VERNEEDNUM - (tag)) /* Reverse order! */
#define DT_VERSIONTAGNUM 16

/* Sun added these machine-independent extensions in the "processor-specific"
   range.  Be compatible.  */
#define DT_AUXILIARY    0x7ffffffd      /* Shared object to load before self */
#define DT_FILTER       0x7fffffff      /* Shared object to get values from */
#define DT_EXTRATAGIDX(tag)     ((Elf32_Word)-((Elf32_Sword) (tag) <<1>>1)-1)
#define DT_EXTRANUM     3

/* Values of `d_un.d_val' in the DT_FLAGS entry.  */
#define DF_ORIGIN       0x00000001      /* Object may use DF_ORIGIN */
#define DF_SYMBOLIC     0x00000002      /* Symbol resolutions starts here */
#define DF_TEXTREL      0x00000004      /* Object contains text relocations */
#define DF_BIND_NOW     0x00000008      /* No lazy binding for this object */
#define DF_STATIC_TLS   0x00000010      /* Module uses the static TLS model */

/* State flags selectable in the `d_un.d_val' element of the DT_FLAGS_1
   entry in the dynamic section.  */
#define DF_1_NOW        0x00000001      /* Set RTLD_NOW for this object.  */
#define DF_1_GLOBAL     0x00000002      /* Set RTLD_GLOBAL for this object.  */
#define DF_1_GROUP      0x00000004      /* Set RTLD_GROUP for this object.  */
#define DF_1_NODELETE   0x00000008      /* Set RTLD_NODELETE for this object.*/
#define DF_1_LOADFLTR   0x00000010      /* Trigger filtee loading at runtime.*/
#define DF_1_INITFIRST  0x00000020      /* Set RTLD_INITFIRST for this object*/
#define DF_1_NOOPEN     0x00000040      /* Set RTLD_NOOPEN for this object.  */
#define DF_1_ORIGIN     0x00000080      /* $ORIGIN must be handled.  */
#define DF_1_DIRECT     0x00000100      /* Direct binding enabled.  */
#define DF_1_TRANS      0x00000200
#define DF_1_INTERPOSE  0x00000400      /* Object is used to interpose.  */
#define DF_1_NODEFLIB   0x00000800      /* Ignore default lib search path.  */
#define DF_1_NODUMP     0x00001000      /* Object can't be dldump'ed.  */
#define DF_1_CONFALT    0x00002000      /* Configuration alternative created.*/
#define DF_1_ENDFILTEE  0x00004000      /* Filtee terminates filters search. */
#define DF_1_DISPRELDNE 0x00008000      /* Disp reloc applied at build time. */
#define DF_1_DISPRELPND 0x00010000      /* Disp reloc applied at run-time.  */
#define DF_1_NODIRECT   0x00020000      /* Object has no-direct binding. */
#define DF_1_IGNMULDEF  0x00040000
#define DF_1_NOKSYMS    0x00080000
#define DF_1_NOHDR      0x00100000
#define DF_1_EDITED     0x00200000      /* Object is modified after built.  */
#define DF_1_NORELOC    0x00400000
#define DF_1_SYMINTPOSE 0x00800000      /* Object has individual interposers.  */
#define DF_1_GLOBAUDIT  0x01000000      /* Global auditing required.  */
#define DF_1_SINGLETON  0x02000000      /* Singleton symbols are used.  */
#define DF_1_STUB       0x04000000
#define DF_1_PIE        0x08000000
#define DF_1_KMOD       0x10000000
#define DF_1_WEAKFILTER 0x20000000
#define DF_1_NOCOMMON   0x40000000

/* Flags for the feature selection in DT_FEATURE_1.  */
#define DTF_1_PARINIT   0x00000001
#define DTF_1_CONFEXP   0x00000002

/* Flags in the DT_POSFLAG_1 entry effecting only the next DT_* entry.  */
#define DF_P1_LAZYLOAD  0x00000001      /* Lazyload following object.  */
#define DF_P1_GROUPPERM 0x00000002      /* Symbols from next object are not
                                           generally available.  */

/* Version definition sections.  */

typedef struct
{
  Elf32_Half    vd_version;             /* Version revision */
  Elf32_Half    vd_flags;               /* Version information */
  Elf32_Half    vd_ndx;                 /* Version Index */
  Elf32_Half    vd_cnt;                 /* Number of associated aux entries */
  Elf32_Word    vd_hash;                /* Version name hash value */
  Elf32_Word    vd_aux;                 /* Offset in bytes to verdaux array */
  Elf32_Word    vd_next;                /* Offset in bytes to next verdef
                                           entry */
} Elf32_Verdef;

typedef struct
{
  Elf64_Half    vd_version;             /* Version revision */
  Elf64_Half    vd_flags;               /* Version information */
  Elf64_Half    vd_ndx;                 /* Version Index */
  Elf64_Half    vd_cnt;                 /* Number of associated aux entries */
  Elf64_Word    vd_hash;                /* Version name hash value */
  Elf64_Word    vd_aux;                 /* Offset in bytes to verdaux array */
  Elf64_Word    vd_next;                /* Offset in bytes to next verdef
                                           entry */
} Elf64_Verdef;


/* Legal values for vd_version (version revision).  */
#define VER_DEF_NONE    0               /* No version */
#define VER_DEF_CURRENT 1               /* Current version */
#define VER_DEF_NUM     2               /* Given version number */

/* Legal values for vd_flags (version information flags).  */
#define VER_FLG_BASE    0x1             /* Version definition of file itself */
#define VER_FLG_WEAK    0x2             /* Weak version identifier */

/* Versym symbol index values.  */
#define VER_NDX_LOCAL           0       /* Symbol is local.  */
#define VER_NDX_GLOBAL          1       /* Symbol is global.  */
#define VER_NDX_LORESERVE       0xff00  /* Beginning of reserved entries.  */
#define VER_NDX_ELIMINATE       0xff01  /* Symbol is to be eliminated.  */

/* Auxiliary version information.  */

typedef struct
{
  Elf32_Word    vda_name;               /* Version or dependency names */
  Elf32_Word    vda_next;               /* Offset in bytes to next verdaux
                                           entry */
} Elf32_Verdaux;

typedef struct
{
  Elf64_Word    vda_name;               /* Version or dependency names */
  Elf64_Word    vda_next;               /* Offset in bytes to next verdaux
                                           entry */
} Elf64_Verdaux;


/* Version dependency section.  */

typedef struct
{
  Elf32_Half    vn_version;             /* Version of structure */
  Elf32_Half    vn_cnt;                 /* Number of associated aux entries */
  Elf32_Word    vn_file;                /* Offset of filename for this
                                           dependency */
  Elf32_Word    vn_aux;                 /* Offset in bytes to vernaux array */
  Elf32_Word    vn_next;                /* Offset in bytes to next verneed
                                           entry */
} Elf32_Verneed;

typedef struct
{
  Elf64_Half    vn_version;             /* Version of structure */
  Elf64_Half    vn_cnt;                 /* Number of associated aux entries */
  Elf64_Word    vn_file;                /* Offset of filename for this
                                           dependency */
  Elf64_Word    vn_aux;                 /* Offset in bytes to vernaux array */
  Elf64_Word    vn_next;                /* Offset in bytes to next verneed
                                           entry */
} Elf64_Verneed;


/* Legal values for vn_version (version revision).  */
#define VER_NEED_NONE    0              /* No version */
#define VER_NEED_CURRENT 1              /* Current version */
#define VER_NEED_NUM     2              /* Given version number */

/* Auxiliary needed version information.  */

typedef struct
{
  Elf32_Word    vna_hash;               /* Hash value of dependency name */
  Elf32_Half    vna_flags;              /* Dependency specific information */
  Elf32_Half    vna_other;              /* Unused */
  Elf32_Word    vna_name;               /* Dependency name string offset */
  Elf32_Word    vna_next;               /* Offset in bytes to next vernaux
                                           entry */
} Elf32_Vernaux;

typedef struct
{
  Elf64_Word    vna_hash;               /* Hash value of dependency name */
  Elf64_Half    vna_flags;              /* Dependency specific information */
  Elf64_Half    vna_other;              /* Unused */
  Elf64_Word    vna_name;               /* Dependency name string offset */
  Elf64_Word    vna_next;               /* Offset in bytes to next vernaux
                                           entry */
} Elf64_Vernaux;


/* Legal values for vna_flags.  */
#define VER_FLG_WEAK    0x2             /* Weak version identifier */


/* Auxiliary vector.  */

/* This vector is normally only used by the program interpreter.  The
   usual definition in an ABI supplement uses the name auxv_t.  The
   vector is not usually defined in a standard <elf.h> file, but it
   can't hurt.  We rename it to avoid conflicts.  The sizes of these
   types are an arrangement between the exec server and the program
   interpreter, so we don't fully specify them here.  */

typedef struct
{
  uint32_t a_type;              /* Entry type */
  union
    {
      uint32_t a_val;           /* Integer value */
      /* We use to have pointer elements added here.  We cannot do that,
         though, since it does not work when using 32-bit definitions
         on 64-bit platforms and vice versa.  */
    } a_un;
} Elf32_auxv_t;

typedef struct
{
  uint64_t a_type;              /* Entry type */
  union
    {
      uint64_t a_val;           /* Integer value */
      /* We use to have pointer elements added here.  We cannot do that,
         though, since it does not work when using 32-bit definitions
         on 64-bit platforms and vice versa.  */
    } a_un;
} Elf64_auxv_t;

#include <bits/auxv.h>
/* Note section contents.  Each entry in the note section begins with
   a header of a fixed form.  */

typedef struct
{
  Elf32_Word n_namesz;                  /* Length of the note's name.  */
  Elf32_Word n_descsz;                  /* Length of the note's descriptor.  */
  Elf32_Word n_type;                    /* Type of the note.  */
} Elf32_Nhdr;

typedef struct
{
  Elf64_Word n_namesz;                  /* Length of the note's name.  */
  Elf64_Word n_descsz;                  /* Length of the note's descriptor.  */
  Elf64_Word n_type;                    /* Type of the note.  */
} Elf64_Nhdr;

/* Known names of notes.  */

/* Solaris entries in the note section have this name.  */
#define ELF_NOTE_SOLARIS        "SUNW Solaris"

/* Note entries for GNU systems have this name.  */
#define ELF_NOTE_GNU            "GNU"

/* Note entries for freedesktop.org have this name.  */
#define ELF_NOTE_FDO            "FDO"

/* Defined types of notes for Solaris.  */

/* Value of descriptor (one word) is desired pagesize for the binary.  */
#define ELF_NOTE_PAGESIZE_HINT  1


/* Defined note types for GNU systems.  */

/* ABI information.  The descriptor consists of words:
   word 0: OS descriptor
   word 1: major version of the ABI
   word 2: minor version of the ABI
   word 3: subminor version of the ABI
*/
#define NT_GNU_ABI_TAG  1
#define ELF_NOTE_ABI    NT_GNU_ABI_TAG /* Old name.  */

/* Known OSes.  These values can appear in word 0 of an
   NT_GNU_ABI_TAG note section entry.  */
#define ELF_NOTE_OS_LINUX       0
#define ELF_NOTE_OS_GNU         1
#define ELF_NOTE_OS_SOLARIS2    2
#define ELF_NOTE_OS_FREEBSD     3

/* Synthetic hwcap information.  The descriptor begins with two words:
   word 0: number of entries
   word 1: bitmask of enabled entries
   Then follow variable-length entries, one byte followed by a
   '\0'-terminated hwcap name string.  The byte gives the bit
   number to test if enabled, (1U << bit) & bitmask.  */
#define NT_GNU_HWCAP    2

/* Build ID bits as generated by ld --build-id.
   The descriptor consists of any nonzero number of bytes.  */
#define NT_GNU_BUILD_ID 3

/* Version note generated by GNU gold containing a version string.  */
#define NT_GNU_GOLD_VERSION     4

/* Program property.  */
#define NT_GNU_PROPERTY_TYPE_0 5

/* Packaging metadata as defined on
   https://systemd.io/COREDUMP_PACKAGE_METADATA/ */
#define NT_FDO_PACKAGING_METADATA 0xcafe1a7e

/* Note section name of program property.   */
#define NOTE_GNU_PROPERTY_SECTION_NAME ".note.gnu.property"

/* Values used in GNU .note.gnu.property notes (NT_GNU_PROPERTY_TYPE_0).  */

/* Stack size.  */
#define GNU_PROPERTY_STACK_SIZE                 1
/* No copy relocation on protected data symbol.  */
#define GNU_PROPERTY_NO_COPY_ON_PROTECTED       2

/* A 4-byte unsigned integer property: A bit is set if it is set in all
   relocatable inputs.  */
#define GNU_PROPERTY_UINT32_AND_LO      0xb0000000
#define GNU_PROPERTY_UINT32_AND_HI      0xb0007fff

/* A 4-byte unsigned integer property: A bit is set if it is set in any
   relocatable inputs.  */
#define GNU_PROPERTY_UINT32_OR_LO       0xb0008000
#define GNU_PROPERTY_UINT32_OR_HI       0xb000ffff

/* The needed properties by the object file.  */
#define GNU_PROPERTY_1_NEEDED           GNU_PROPERTY_UINT32_OR_LO

/* Set if the object file requires canonical function pointers and
   cannot be used with copy relocation.  */
#define GNU_PROPERTY_1_NEEDED_INDIRECT_EXTERN_ACCESS (1U << 0)

/* Processor-specific semantics, lo */
#define GNU_PROPERTY_LOPROC                     0xc0000000
/* Processor-specific semantics, hi */
#define GNU_PROPERTY_HIPROC                     0xdfffffff
/* Application-specific semantics, lo */
#define GNU_PROPERTY_LOUSER                     0xe0000000
/* Application-specific semantics, hi */
#define GNU_PROPERTY_HIUSER                     0xffffffff

/* AArch64 specific GNU properties.  */
#define GNU_PROPERTY_AARCH64_FEATURE_1_AND      0xc0000000

#define GNU_PROPERTY_AARCH64_FEATURE_1_BTI      (1U << 0)
#define GNU_PROPERTY_AARCH64_FEATURE_1_PAC      (1U << 1)

/* The x86 instruction sets indicated by the corresponding bits are
   used in program.  Their support in the hardware is optional.  */
#define GNU_PROPERTY_X86_ISA_1_USED             0xc0010002
/* The x86 instruction sets indicated by the corresponding bits are
   used in program and they must be supported by the hardware.   */
#define GNU_PROPERTY_X86_ISA_1_NEEDED           0xc0008002
/* X86 processor-specific features used in program.  */
#define GNU_PROPERTY_X86_FEATURE_1_AND          0xc0000002

/* GNU_PROPERTY_X86_ISA_1_BASELINE: CMOV, CX8 (cmpxchg8b), FPU (fld),
   MMX, OSFXSR (fxsave), SCE (syscall), SSE and SSE2.  */
#define GNU_PROPERTY_X86_ISA_1_BASELINE         (1U << 0)
/* GNU_PROPERTY_X86_ISA_1_V2: GNU_PROPERTY_X86_ISA_1_BASELINE,
   CMPXCHG16B (cmpxchg16b), LAHF-SAHF (lahf), POPCNT (popcnt), SSE3,
   SSSE3, SSE4.1 and SSE4.2.  */
#define GNU_PROPERTY_X86_ISA_1_V2               (1U << 1)
/* GNU_PROPERTY_X86_ISA_1_V3: GNU_PROPERTY_X86_ISA_1_V2, AVX, AVX2, BMI1,
   BMI2, F16C, FMA, LZCNT, MOVBE, XSAVE.  */
#define GNU_PROPERTY_X86_ISA_1_V3               (1U << 2)
/* GNU_PROPERTY_X86_ISA_1_V4: GNU_PROPERTY_X86_ISA_1_V3, AVX512F,
   AVX512BW, AVX512CD, AVX512DQ and AVX512VL.  */
#define GNU_PROPERTY_X86_ISA_1_V4               (1U << 3)

/* This indicates that all executable sections are compatible with
   IBT.  */
#define GNU_PROPERTY_X86_FEATURE_1_IBT          (1U << 0)
/* This indicates that all executable sections are compatible with
   SHSTK.  */
#define GNU_PROPERTY_X86_FEATURE_1_SHSTK        (1U << 1)

/* Move records.  */
typedef struct
{
  Elf32_Xword m_value;          /* Symbol value.  */
  Elf32_Word m_info;            /* Size and index.  */
  Elf32_Word m_poffset;         /* Symbol offset.  */
  Elf32_Half m_repeat;          /* Repeat count.  */
  Elf32_Half m_stride;          /* Stride info.  */
} Elf32_Move;

typedef struct
{
  Elf64_Xword m_value;          /* Symbol value.  */
  Elf64_Xword m_info;           /* Size and index.  */
  Elf64_Xword m_poffset;        /* Symbol offset.  */
  Elf64_Half m_repeat;          /* Repeat count.  */
  Elf64_Half m_stride;          /* Stride info.  */
} Elf64_Move;

/* Macro to construct move records.  */
#define ELF32_M_SYM(info)       ((info) >> 8)
#define ELF32_M_SIZE(info)      ((unsigned char) (info))
#define ELF32_M_INFO(sym, size) (((sym) << 8) + (unsigned char) (size))

#define ELF64_M_SYM(info)       ELF32_M_SYM (info)
#define ELF64_M_SIZE(info)      ELF32_M_SIZE (info)
#define ELF64_M_INFO(sym, size) ELF32_M_INFO (sym, size)


/* Motorola 68k specific definitions.  */

/* Values for Elf32_Ehdr.e_flags.  */
#define EF_CPU32        0x00810000

/* m68k relocs.  */

#define R_68K_NONE      0               /* No reloc */
#define R_68K_32        1               /* Direct 32 bit  */
#define R_68K_16        2               /* Direct 16 bit  */
#define R_68K_8         3               /* Direct 8 bit  */
#define R_68K_PC32      4               /* PC relative 32 bit */
#define R_68K_PC16      5               /* PC relative 16 bit */
#define R_68K_PC8       6               /* PC relative 8 bit */
#define R_68K_GOT32     7               /* 32 bit PC relative GOT entry */
#define R_68K_GOT16     8               /* 16 bit PC relative GOT entry */
#define R_68K_GOT8      9               /* 8 bit PC relative GOT entry */
#define R_68K_GOT32O    10              /* 32 bit GOT offset */
#define R_68K_GOT16O    11              /* 16 bit GOT offset */
#define R_68K_GOT8O     12              /* 8 bit GOT offset */
#define R_68K_PLT32     13              /* 32 bit PC relative PLT address */
#define R_68K_PLT16     14              /* 16 bit PC relative PLT address */
#define R_68K_PLT8      15              /* 8 bit PC relative PLT address */
#define R_68K_PLT32O    16              /* 32 bit PLT offset */
#define R_68K_PLT16O    17              /* 16 bit PLT offset */
#define R_68K_PLT8O     18              /* 8 bit PLT offset */
#define R_68K_COPY      19              /* Copy symbol at runtime */
#define R_68K_GLOB_DAT  20              /* Create GOT entry */
#define R_68K_JMP_SLOT  21              /* Create PLT entry */
#define R_68K_RELATIVE  22              /* Adjust by program base */
#define R_68K_TLS_GD32      25          /* 32 bit GOT offset for GD */
#define R_68K_TLS_GD16      26          /* 16 bit GOT offset for GD */
#define R_68K_TLS_GD8       27          /* 8 bit GOT offset for GD */
#define R_68K_TLS_LDM32     28          /* 32 bit GOT offset for LDM */
#define R_68K_TLS_LDM16     29          /* 16 bit GOT offset for LDM */
#define R_68K_TLS_LDM8      30          /* 8 bit GOT offset for LDM */
#define R_68K_TLS_LDO32     31          /* 32 bit module-relative offset */
#define R_68K_TLS_LDO16     32          /* 16 bit module-relative offset */
#define R_68K_TLS_LDO8      33          /* 8 bit module-relative offset */
#define R_68K_TLS_IE32      34          /* 32 bit GOT offset for IE */
#define R_68K_TLS_IE16      35          /* 16 bit GOT offset for IE */
#define R_68K_TLS_IE8       36          /* 8 bit GOT offset for IE */
#define R_68K_TLS_LE32      37          /* 32 bit offset relative to
                                           static TLS block */
#define R_68K_TLS_LE16      38          /* 16 bit offset relative to
                                           static TLS block */
#define R_68K_TLS_LE8       39          /* 8 bit offset relative to
                                           static TLS block */
#define R_68K_TLS_DTPMOD32  40          /* 32 bit module number */
#define R_68K_TLS_DTPREL32  41          /* 32 bit module-relative offset */
#define R_68K_TLS_TPREL32   42          /* 32 bit TP-relative offset */
/* Keep this the last entry.  */
#define R_68K_NUM       43

/* Intel 80386 specific definitions.  */

/* i386 relocs.  */

#define R_386_NONE         0            /* No reloc */
#define R_386_32           1            /* Direct 32 bit  */
#define R_386_PC32         2            /* PC relative 32 bit */
#define R_386_GOT32        3            /* 32 bit GOT entry */
#define R_386_PLT32        4            /* 32 bit PLT address */
#define R_386_COPY         5            /* Copy symbol at runtime */
#define R_386_GLOB_DAT     6            /* Create GOT entry */
#define R_386_JMP_SLOT     7            /* Create PLT entry */
#define R_386_RELATIVE     8            /* Adjust by program base */
#define R_386_GOTOFF       9            /* 32 bit offset to GOT */
#define R_386_GOTPC        10           /* 32 bit PC relative offset to GOT */
#define R_386_32PLT        11
#define R_386_TLS_TPOFF    14           /* Offset in static TLS block */
#define R_386_TLS_IE       15           /* Address of GOT entry for static TLS
                                           block offset */
#define R_386_TLS_GOTIE    16           /* GOT entry for static TLS block
                                           offset */
#define R_386_TLS_LE       17           /* Offset relative to static TLS
                                           block */
#define R_386_TLS_GD       18           /* Direct 32 bit for GNU version of
                                           general dynamic thread local data */
#define R_386_TLS_LDM      19           /* Direct 32 bit for GNU version of
                                           local dynamic thread local data
                                           in LE code */
#define R_386_16           20
#define R_386_PC16         21
#define R_386_8            22
#define R_386_PC8          23
#define R_386_TLS_GD_32    24           /* Direct 32 bit for general dynamic
                                           thread local data */
#define R_386_TLS_GD_PUSH  25           /* Tag for pushl in GD TLS code */
#define R_386_TLS_GD_CALL  26           /* Relocation for call to
                                           __tls_get_addr() */
#define R_386_TLS_GD_POP   27           /* Tag for popl in GD TLS code */
#define R_386_TLS_LDM_32   28           /* Direct 32 bit for local dynamic
                                           thread local data in LE code */
#define R_386_TLS_LDM_PUSH 29           /* Tag for pushl in LDM TLS code */
#define R_386_TLS_LDM_CALL 30           /* Relocation for call to
                                           __tls_get_addr() in LDM code */
#define R_386_TLS_LDM_POP  31           /* Tag for popl in LDM TLS code */
#define R_386_TLS_LDO_32   32           /* Offset relative to TLS block */
#define R_386_TLS_IE_32    33           /* GOT entry for negated static TLS
                                           block offset */
#define R_386_TLS_LE_32    34           /* Negated offset relative to static
                                           TLS block */
#define R_386_TLS_DTPMOD32 35           /* ID of module containing symbol */
#define R_386_TLS_DTPOFF32 36           /* Offset in TLS block */
#define R_386_TLS_TPOFF32  37           /* Negated offset in static TLS block */
#define R_386_SIZE32       38           /* 32-bit symbol size */
#define R_386_TLS_GOTDESC  39           /* GOT offset for TLS descriptor.  */
#define R_386_TLS_DESC_CALL 40          /* Marker of call through TLS
                                           descriptor for
                                           relaxation.  */
#define R_386_TLS_DESC     41           /* TLS descriptor containing
                                           pointer to code and to
                                           argument, returning the TLS
                                           offset for the symbol.  */
#define R_386_IRELATIVE    42           /* Adjust indirectly by program base */
#define R_386_GOT32X       43           /* Load from 32 bit GOT entry,
                                           relaxable. */
/* Keep this the last entry.  */
#define R_386_NUM          44

/* SUN SPARC specific definitions.  */

/* Legal values for ST_TYPE subfield of st_info (symbol type).  */

#define STT_SPARC_REGISTER      13      /* Global register reserved to app. */

/* Values for Elf64_Ehdr.e_flags.  */

#define EF_SPARCV9_MM           3
#define EF_SPARCV9_TSO          0
#define EF_SPARCV9_PSO          1
#define EF_SPARCV9_RMO          2
#define EF_SPARC_LEDATA         0x800000 /* little endian data */
#define EF_SPARC_EXT_MASK       0xFFFF00
#define EF_SPARC_32PLUS         0x000100 /* generic V8+ features */
#define EF_SPARC_SUN_US1        0x000200 /* Sun UltraSPARC1 extensions */
#define EF_SPARC_HAL_R1         0x000400 /* HAL R1 extensions */
#define EF_SPARC_SUN_US3        0x000800 /* Sun UltraSPARCIII extensions */

/* SPARC relocs.  */

#define R_SPARC_NONE            0       /* No reloc */
#define R_SPARC_8               1       /* Direct 8 bit */
#define R_SPARC_16              2       /* Direct 16 bit */
#define R_SPARC_32              3       /* Direct 32 bit */
#define R_SPARC_DISP8           4       /* PC relative 8 bit */
#define R_SPARC_DISP16          5       /* PC relative 16 bit */
#define R_SPARC_DISP32          6       /* PC relative 32 bit */
#define R_SPARC_WDISP30         7       /* PC relative 30 bit shifted */
#define R_SPARC_WDISP22         8       /* PC relative 22 bit shifted */
#define R_SPARC_HI22            9       /* High 22 bit */
#define R_SPARC_22              10      /* Direct 22 bit */
#define R_SPARC_13              11      /* Direct 13 bit */
#define R_SPARC_LO10            12      /* Truncated 10 bit */
#define R_SPARC_GOT10           13      /* Truncated 10 bit GOT entry */
#define R_SPARC_GOT13           14      /* 13 bit GOT entry */
#define R_SPARC_GOT22           15      /* 22 bit GOT entry shifted */
#define R_SPARC_PC10            16      /* PC relative 10 bit truncated */
#define R_SPARC_PC22            17      /* PC relative 22 bit shifted */
#define R_SPARC_WPLT30          18      /* 30 bit PC relative PLT address */
#define R_SPARC_COPY            19      /* Copy symbol at runtime */
#define R_SPARC_GLOB_DAT        20      /* Create GOT entry */
#define R_SPARC_JMP_SLOT        21      /* Create PLT entry */
#define R_SPARC_RELATIVE        22      /* Adjust by program base */
#define R_SPARC_UA32            23      /* Direct 32 bit unaligned */

/* Additional Sparc64 relocs.  */

#define R_SPARC_PLT32           24      /* Direct 32 bit ref to PLT entry */
#define R_SPARC_HIPLT22         25      /* High 22 bit PLT entry */
#define R_SPARC_LOPLT10         26      /* Truncated 10 bit PLT entry */
#define R_SPARC_PCPLT32         27      /* PC rel 32 bit ref to PLT entry */
#define R_SPARC_PCPLT22         28      /* PC rel high 22 bit PLT entry */
#define R_SPARC_PCPLT10         29      /* PC rel trunc 10 bit PLT entry */
#define R_SPARC_10              30      /* Direct 10 bit */
#define R_SPARC_11              31      /* Direct 11 bit */
#define R_SPARC_64              32      /* Direct 64 bit */
#define R_SPARC_OLO10           33      /* 10bit with secondary 13bit addend */
#define R_SPARC_HH22            34      /* Top 22 bits of direct 64 bit */
#define R_SPARC_HM10            35      /* High middle 10 bits of ... */
#define R_SPARC_LM22            36      /* Low middle 22 bits of ... */
#define R_SPARC_PC_HH22         37      /* Top 22 bits of pc rel 64 bit */
#define R_SPARC_PC_HM10         38      /* High middle 10 bit of ... */
#define R_SPARC_PC_LM22         39      /* Low miggle 22 bits of ... */
#define R_SPARC_WDISP16         40      /* PC relative 16 bit shifted */
#define R_SPARC_WDISP19         41      /* PC relative 19 bit shifted */
#define R_SPARC_GLOB_JMP        42      /* was part of v9 ABI but was removed */
#define R_SPARC_7               43      /* Direct 7 bit */
#define R_SPARC_5               44      /* Direct 5 bit */
#define R_SPARC_6               45      /* Direct 6 bit */
#define R_SPARC_DISP64          46      /* PC relative 64 bit */
#define R_SPARC_PLT64           47      /* Direct 64 bit ref to PLT entry */
#define R_SPARC_HIX22           48      /* High 22 bit complemented */
#define R_SPARC_LOX10           49      /* Truncated 11 bit complemented */
#define R_SPARC_H44             50      /* Direct high 12 of 44 bit */
#define R_SPARC_M44             51      /* Direct mid 22 of 44 bit */
#define R_SPARC_L44             52      /* Direct low 10 of 44 bit */
#define R_SPARC_REGISTER        53      /* Global register usage */
#define R_SPARC_UA64            54      /* Direct 64 bit unaligned */
#define R_SPARC_UA16            55      /* Direct 16 bit unaligned */
#define R_SPARC_TLS_GD_HI22     56
#define R_SPARC_TLS_GD_LO10     57
#define R_SPARC_TLS_GD_ADD      58
#define R_SPARC_TLS_GD_CALL     59
#define R_SPARC_TLS_LDM_HI22    60
#define R_SPARC_TLS_LDM_LO10    61
#define R_SPARC_TLS_LDM_ADD     62
#define R_SPARC_TLS_LDM_CALL    63
#define R_SPARC_TLS_LDO_HIX22   64
#define R_SPARC_TLS_LDO_LOX10   65
#define R_SPARC_TLS_LDO_ADD     66
#define R_SPARC_TLS_IE_HI22     67
#define R_SPARC_TLS_IE_LO10     68
#define R_SPARC_TLS_IE_LD       69
#define R_SPARC_TLS_IE_LDX      70
#define R_SPARC_TLS_IE_ADD      71
#define R_SPARC_TLS_LE_HIX22    72
#define R_SPARC_TLS_LE_LOX10    73
#define R_SPARC_TLS_DTPMOD32    74
#define R_SPARC_TLS_DTPMOD64    75
#define R_SPARC_TLS_DTPOFF32    76
#define R_SPARC_TLS_DTPOFF64    77
#define R_SPARC_TLS_TPOFF32     78
#define R_SPARC_TLS_TPOFF64     79
#define R_SPARC_GOTDATA_HIX22   80
#define R_SPARC_GOTDATA_LOX10   81
#define R_SPARC_GOTDATA_OP_HIX22        82
#define R_SPARC_GOTDATA_OP_LOX10        83
#define R_SPARC_GOTDATA_OP      84
#define R_SPARC_H34             85
#define R_SPARC_SIZE32          86
#define R_SPARC_SIZE64          87
#define R_SPARC_WDISP10         88
#define R_SPARC_JMP_IREL        248
#define R_SPARC_IRELATIVE       249
#define R_SPARC_GNU_VTINHERIT   250
#define R_SPARC_GNU_VTENTRY     251
#define R_SPARC_REV32           252
/* Keep this the last entry.  */
#define R_SPARC_NUM             253

/* For Sparc64, legal values for d_tag of Elf64_Dyn.  */

#define DT_SPARC_REGISTER       0x70000001
#define DT_SPARC_NUM            2

/* MIPS R3000 specific definitions.  */

/* Legal values for e_flags field of Elf32_Ehdr.  */

#define EF_MIPS_NOREORDER       1     /* A .noreorder directive was used.  */
#define EF_MIPS_PIC             2     /* Contains PIC code.  */
#define EF_MIPS_CPIC            4     /* Uses PIC calling sequence.  */
#define EF_MIPS_XGOT            8
#define EF_MIPS_64BIT_WHIRL     16
#define EF_MIPS_ABI2            32
#define EF_MIPS_ABI_ON32        64
#define EF_MIPS_FP64            512  /* Uses FP64 (12 callee-saved).  */
#define EF_MIPS_NAN2008 1024  /* Uses IEEE 754-2008 NaN encoding.  */
#define EF_MIPS_ARCH            0xf0000000 /* MIPS architecture level.  */

/* Legal values for MIPS architecture level.  */

#define EF_MIPS_ARCH_1          0x00000000 /* -mips1 code.  */
#define EF_MIPS_ARCH_2          0x10000000 /* -mips2 code.  */
#define EF_MIPS_ARCH_3          0x20000000 /* -mips3 code.  */
#define EF_MIPS_ARCH_4          0x30000000 /* -mips4 code.  */
#define EF_MIPS_ARCH_5          0x40000000 /* -mips5 code.  */
#define EF_MIPS_ARCH_32         0x50000000 /* MIPS32 code.  */
#define EF_MIPS_ARCH_64         0x60000000 /* MIPS64 code.  */
#define EF_MIPS_ARCH_32R2       0x70000000 /* MIPS32r2 code.  */
#define EF_MIPS_ARCH_64R2       0x80000000 /* MIPS64r2 code.  */

/* The following are unofficial names and should not be used.  */

#define E_MIPS_ARCH_1           EF_MIPS_ARCH_1
#define E_MIPS_ARCH_2           EF_MIPS_ARCH_2
#define E_MIPS_ARCH_3           EF_MIPS_ARCH_3
#define E_MIPS_ARCH_4           EF_MIPS_ARCH_4
#define E_MIPS_ARCH_5           EF_MIPS_ARCH_5
#define E_MIPS_ARCH_32          EF_MIPS_ARCH_32
#define E_MIPS_ARCH_64          EF_MIPS_ARCH_64

/* Special section indices.  */

#define SHN_MIPS_ACOMMON        0xff00  /* Allocated common symbols.  */
#define SHN_MIPS_TEXT           0xff01  /* Allocated test symbols.  */
#define SHN_MIPS_DATA           0xff02  /* Allocated data symbols.  */
#define SHN_MIPS_SCOMMON        0xff03  /* Small common symbols.  */
#define SHN_MIPS_SUNDEFINED     0xff04  /* Small undefined symbols.  */

/* Legal values for sh_type field of Elf32_Shdr.  */

#define SHT_MIPS_LIBLIST        0x70000000 /* Shared objects used in link.  */
#define SHT_MIPS_MSYM           0x70000001
#define SHT_MIPS_CONFLICT       0x70000002 /* Conflicting symbols.  */
#define SHT_MIPS_GPTAB          0x70000003 /* Global data area sizes.  */
#define SHT_MIPS_UCODE          0x70000004 /* Reserved for SGI/MIPS compilers */
#define SHT_MIPS_DEBUG          0x70000005 /* MIPS ECOFF debugging info.  */
#define SHT_MIPS_REGINFO        0x70000006 /* Register usage information.  */
#define SHT_MIPS_PACKAGE        0x70000007
#define SHT_MIPS_PACKSYM        0x70000008
#define SHT_MIPS_RELD           0x70000009
#define SHT_MIPS_IFACE          0x7000000b
#define SHT_MIPS_CONTENT        0x7000000c
#define SHT_MIPS_OPTIONS        0x7000000d /* Miscellaneous options.  */
#define SHT_MIPS_SHDR           0x70000010
#define SHT_MIPS_FDESC          0x70000011
#define SHT_MIPS_EXTSYM         0x70000012
#define SHT_MIPS_DENSE          0x70000013
#define SHT_MIPS_PDESC          0x70000014
#define SHT_MIPS_LOCSYM         0x70000015
#define SHT_MIPS_AUXSYM         0x70000016
#define SHT_MIPS_OPTSYM         0x70000017
#define SHT_MIPS_LOCSTR         0x70000018
#define SHT_MIPS_LINE           0x70000019
#define SHT_MIPS_RFDESC         0x7000001a
#define SHT_MIPS_DELTASYM       0x7000001b
#define SHT_MIPS_DELTAINST      0x7000001c
#define SHT_MIPS_DELTACLASS     0x7000001d
#define SHT_MIPS_DWARF          0x7000001e /* DWARF debugging information.  */
#define SHT_MIPS_DELTADECL      0x7000001f
#define SHT_MIPS_SYMBOL_LIB     0x70000020
#define SHT_MIPS_EVENTS         0x70000021 /* Event section.  */
#define SHT_MIPS_TRANSLATE      0x70000022
#define SHT_MIPS_PIXIE          0x70000023
#define SHT_MIPS_XLATE          0x70000024
#define SHT_MIPS_XLATE_DEBUG    0x70000025
#define SHT_MIPS_WHIRL          0x70000026
#define SHT_MIPS_EH_REGION      0x70000027
#define SHT_MIPS_XLATE_OLD      0x70000028
#define SHT_MIPS_PDR_EXCEPTION  0x70000029
#define SHT_MIPS_XHASH          0x7000002b

/* Legal values for sh_flags field of Elf32_Shdr.  */

#define SHF_MIPS_GPREL          0x10000000 /* Must be in global data area.  */
#define SHF_MIPS_MERGE          0x20000000
#define SHF_MIPS_ADDR           0x40000000
#define SHF_MIPS_STRINGS        0x80000000
#define SHF_MIPS_NOSTRIP        0x08000000
#define SHF_MIPS_LOCAL          0x04000000
#define SHF_MIPS_NAMES          0x02000000
#define SHF_MIPS_NODUPE         0x01000000


/* Symbol tables.  */

/* MIPS specific values for `st_other'.  */
#define STO_MIPS_DEFAULT                0x0
#define STO_MIPS_INTERNAL               0x1
#define STO_MIPS_HIDDEN                 0x2
#define STO_MIPS_PROTECTED              0x3
#define STO_MIPS_PLT                    0x8
#define STO_MIPS_SC_ALIGN_UNUSED        0xff

/* MIPS specific values for `st_info'.  */
#define STB_MIPS_SPLIT_COMMON           13

/* Entries found in sections of type SHT_MIPS_GPTAB.  */

typedef union
{
  struct
    {
      Elf32_Word gt_current_g_value;    /* -G value used for compilation.  */
      Elf32_Word gt_unused;             /* Not used.  */
    } gt_header;                        /* First entry in section.  */
  struct
    {
      Elf32_Word gt_g_value;            /* If this value were used for -G.  */
      Elf32_Word gt_bytes;              /* This many bytes would be used.  */
    } gt_entry;                         /* Subsequent entries in section.  */
} Elf32_gptab;

/* Entry found in sections of type SHT_MIPS_REGINFO.  */

typedef struct
{
  Elf32_Word ri_gprmask;                /* General registers used.  */
  Elf32_Word ri_cprmask[4];             /* Coprocessor registers used.  */
  Elf32_Sword ri_gp_value;              /* $gp register value.  */
} Elf32_RegInfo;

/* Entries found in sections of type SHT_MIPS_OPTIONS.  */

typedef struct
{
  unsigned char kind;           /* Determines interpretation of the
                                   variable part of descriptor.  */
  unsigned char size;           /* Size of descriptor, including header.  */
  Elf32_Section section;        /* Section header index of section affected,
                                   0 for global options.  */
  Elf32_Word info;              /* Kind-specific information.  */
} Elf_Options;

/* Values for `kind' field in Elf_Options.  */

#define ODK_NULL        0       /* Undefined.  */
#define ODK_REGINFO     1       /* Register usage information.  */
#define ODK_EXCEPTIONS  2       /* Exception processing options.  */
#define ODK_PAD         3       /* Section padding options.  */
#define ODK_HWPATCH     4       /* Hardware workarounds performed */
#define ODK_FILL        5       /* record the fill value used by the linker. */
#define ODK_TAGS        6       /* reserve space for desktop tools to write. */
#define ODK_HWAND       7       /* HW workarounds.  'AND' bits when merging. */
#define ODK_HWOR        8       /* HW workarounds.  'OR' bits when merging.  */

/* Values for `info' in Elf_Options for ODK_EXCEPTIONS entries.  */

#define OEX_FPU_MIN     0x1f    /* FPE's which MUST be enabled.  */
#define OEX_FPU_MAX     0x1f00  /* FPE's which MAY be enabled.  */
#define OEX_PAGE0       0x10000 /* page zero must be mapped.  */
#define OEX_SMM         0x20000 /* Force sequential memory mode?  */
#define OEX_FPDBUG      0x40000 /* Force floating point debug mode?  */
#define OEX_PRECISEFP   OEX_FPDBUG
#define OEX_DISMISS     0x80000 /* Dismiss invalid address faults?  */

#define OEX_FPU_INVAL   0x10
#define OEX_FPU_DIV0    0x08
#define OEX_FPU_OFLO    0x04
#define OEX_FPU_UFLO    0x02
#define OEX_FPU_INEX    0x01

/* Masks for `info' in Elf_Options for an ODK_HWPATCH entry.  */

#define OHW_R4KEOP      0x1     /* R4000 end-of-page patch.  */
#define OHW_R8KPFETCH   0x2     /* may need R8000 prefetch patch.  */
#define OHW_R5KEOP      0x4     /* R5000 end-of-page patch.  */
#define OHW_R5KCVTL     0x8     /* R5000 cvt.[ds].l bug.  clean=1.  */

#define OPAD_PREFIX     0x1
#define OPAD_POSTFIX    0x2
#define OPAD_SYMBOL     0x4

/* Entry found in `.options' section.  */

typedef struct
{
  Elf32_Word hwp_flags1;        /* Extra flags.  */
  Elf32_Word hwp_flags2;        /* Extra flags.  */
} Elf_Options_Hw;

/* Masks for `info' in ElfOptions for ODK_HWAND and ODK_HWOR entries.  */

#define OHWA0_R4KEOP_CHECKED    0x00000001
#define OHWA1_R4KEOP_CLEAN      0x00000002

/* MIPS relocs.  */

#define R_MIPS_NONE             0       /* No reloc */
#define R_MIPS_16               1       /* Direct 16 bit */
#define R_MIPS_32               2       /* Direct 32 bit */
#define R_MIPS_REL32            3       /* PC relative 32 bit */
#define R_MIPS_26               4       /* Direct 26 bit shifted */
#define R_MIPS_HI16             5       /* High 16 bit */
#define R_MIPS_LO16             6       /* Low 16 bit */
#define R_MIPS_GPREL16          7       /* GP relative 16 bit */
#define R_MIPS_LITERAL          8       /* 16 bit literal entry */
#define R_MIPS_GOT16            9       /* 16 bit GOT entry */
#define R_MIPS_PC16             10      /* PC relative 16 bit */
#define R_MIPS_CALL16           11      /* 16 bit GOT entry for function */
#define R_MIPS_GPREL32          12      /* GP relative 32 bit */

#define R_MIPS_SHIFT5           16
#define R_MIPS_SHIFT6           17
#define R_MIPS_64               18
#define R_MIPS_GOT_DISP         19
#define R_MIPS_GOT_PAGE         20
#define R_MIPS_GOT_OFST         21
#define R_MIPS_GOT_HI16         22
#define R_MIPS_GOT_LO16         23
#define R_MIPS_SUB              24
#define R_MIPS_INSERT_A         25
#define R_MIPS_INSERT_B         26
#define R_MIPS_DELETE           27
#define R_MIPS_HIGHER           28
#define R_MIPS_HIGHEST          29
#define R_MIPS_CALL_HI16        30
#define R_MIPS_CALL_LO16        31
#define R_MIPS_SCN_DISP         32
#define R_MIPS_REL16            33
#define R_MIPS_ADD_IMMEDIATE    34
#define R_MIPS_PJUMP            35
#define R_MIPS_RELGOT           36
#define R_MIPS_JALR             37
#define R_MIPS_TLS_DTPMOD32     38      /* Module number 32 bit */
#define R_MIPS_TLS_DTPREL32     39      /* Module-relative offset 32 bit */
#define R_MIPS_TLS_DTPMOD64     40      /* Module number 64 bit */
#define R_MIPS_TLS_DTPREL64     41      /* Module-relative offset 64 bit */
#define R_MIPS_TLS_GD           42      /* 16 bit GOT offset for GD */
#define R_MIPS_TLS_LDM          43      /* 16 bit GOT offset for LDM */
#define R_MIPS_TLS_DTPREL_HI16  44      /* Module-relative offset, high 16 bits */
#define R_MIPS_TLS_DTPREL_LO16  45      /* Module-relative offset, low 16 bits */
#define R_MIPS_TLS_GOTTPREL     46      /* 16 bit GOT offset for IE */
#define R_MIPS_TLS_TPREL32      47      /* TP-relative offset, 32 bit */
#define R_MIPS_TLS_TPREL64      48      /* TP-relative offset, 64 bit */
#define R_MIPS_TLS_TPREL_HI16   49      /* TP-relative offset, high 16 bits */
#define R_MIPS_TLS_TPREL_LO16   50      /* TP-relative offset, low 16 bits */
#define R_MIPS_GLOB_DAT         51
#define R_MIPS_COPY             126
#define R_MIPS_JUMP_SLOT        127
/* Keep this the last entry.  */
#define R_MIPS_NUM              128

/* Legal values for p_type field of Elf32_Phdr.  */

#define PT_MIPS_REGINFO   0x70000000    /* Register usage information. */
#define PT_MIPS_RTPROC    0x70000001    /* Runtime procedure table. */
#define PT_MIPS_OPTIONS   0x70000002
#define PT_MIPS_ABIFLAGS  0x70000003    /* FP mode requirement. */

/* Special program header types.  */

#define PF_MIPS_LOCAL   0x10000000

/* Legal values for d_tag field of Elf32_Dyn.  */

#define DT_MIPS_RLD_VERSION  0x70000001 /* Runtime linker interface version */
#define DT_MIPS_TIME_STAMP   0x70000002 /* Timestamp */
#define DT_MIPS_ICHECKSUM    0x70000003 /* Checksum */
#define DT_MIPS_IVERSION     0x70000004 /* Version string (string tbl index) */
#define DT_MIPS_FLAGS        0x70000005 /* Flags */
#define DT_MIPS_BASE_ADDRESS 0x70000006 /* Base address */
#define DT_MIPS_MSYM         0x70000007
#define DT_MIPS_CONFLICT     0x70000008 /* Address of CONFLICT section */
#define DT_MIPS_LIBLIST      0x70000009 /* Address of LIBLIST section */
#define DT_MIPS_LOCAL_GOTNO  0x7000000a /* Number of local GOT entries */
#define DT_MIPS_CONFLICTNO   0x7000000b /* Number of CONFLICT entries */
#define DT_MIPS_LIBLISTNO    0x70000010 /* Number of LIBLIST entries */
#define DT_MIPS_SYMTABNO     0x70000011 /* Number of DYNSYM entries */
#define DT_MIPS_UNREFEXTNO   0x70000012 /* First external DYNSYM */
#define DT_MIPS_GOTSYM       0x70000013 /* First GOT entry in DYNSYM */
#define DT_MIPS_HIPAGENO     0x70000014 /* Number of GOT page table entries */
#define DT_MIPS_RLD_MAP      0x70000016 /* Address of run time loader map.  */
#define DT_MIPS_DELTA_CLASS  0x70000017 /* Delta C++ class definition.  */
#define DT_MIPS_DELTA_CLASS_NO    0x70000018 /* Number of entries in
                                                DT_MIPS_DELTA_CLASS.  */
#define DT_MIPS_DELTA_INSTANCE    0x70000019 /* Delta C++ class instances.  */
#define DT_MIPS_DELTA_INSTANCE_NO 0x7000001a /* Number of entries in
                                                DT_MIPS_DELTA_INSTANCE.  */
#define DT_MIPS_DELTA_RELOC  0x7000001b /* Delta relocations.  */
#define DT_MIPS_DELTA_RELOC_NO 0x7000001c /* Number of entries in
                                             DT_MIPS_DELTA_RELOC.  */
#define DT_MIPS_DELTA_SYM    0x7000001d /* Delta symbols that Delta
                                           relocations refer to.  */
#define DT_MIPS_DELTA_SYM_NO 0x7000001e /* Number of entries in
                                           DT_MIPS_DELTA_SYM.  */
#define DT_MIPS_DELTA_CLASSSYM 0x70000020 /* Delta symbols that hold the
                                             class declaration.  */
#define DT_MIPS_DELTA_CLASSSYM_NO 0x70000021 /* Number of entries in
                                                DT_MIPS_DELTA_CLASSSYM.  */
#define DT_MIPS_CXX_FLAGS    0x70000022 /* Flags indicating for C++ flavor.  */
#define DT_MIPS_PIXIE_INIT   0x70000023
#define DT_MIPS_SYMBOL_LIB   0x70000024
#define DT_MIPS_LOCALPAGE_GOTIDX 0x70000025
#define DT_MIPS_LOCAL_GOTIDX 0x70000026
#define DT_MIPS_HIDDEN_GOTIDX 0x70000027
#define DT_MIPS_PROTECTED_GOTIDX 0x70000028
#define DT_MIPS_OPTIONS      0x70000029 /* Address of .options.  */
#define DT_MIPS_INTERFACE    0x7000002a /* Address of .interface.  */
#define DT_MIPS_DYNSTR_ALIGN 0x7000002b
#define DT_MIPS_INTERFACE_SIZE 0x7000002c /* Size of the .interface section. */
#define DT_MIPS_RLD_TEXT_RESOLVE_ADDR 0x7000002d /* Address of rld_text_rsolve
                                                    function stored in GOT.  */
#define DT_MIPS_PERF_SUFFIX  0x7000002e /* Default suffix of dso to be added
                                           by rld on dlopen() calls.  */
#define DT_MIPS_COMPACT_SIZE 0x7000002f /* (O32)Size of compact rel section. */
#define DT_MIPS_GP_VALUE     0x70000030 /* GP value for aux GOTs.  */
#define DT_MIPS_AUX_DYNAMIC  0x70000031 /* Address of aux .dynamic.  */
/* The address of .got.plt in an executable using the new non-PIC ABI.  */
#define DT_MIPS_PLTGOT       0x70000032
/* The base of the PLT in an executable using the new non-PIC ABI if that
   PLT is writable.  For a non-writable PLT, this is omitted or has a zero
   value.  */
#define DT_MIPS_RWPLT        0x70000034
/* An alternative description of the classic MIPS RLD_MAP that is usable
   in a PIE as it stores a relative offset from the address of the tag
   rather than an absolute address.  */
#define DT_MIPS_RLD_MAP_REL  0x70000035
/* GNU-style hash table with xlat.  */
#define DT_MIPS_XHASH        0x70000036
#define DT_MIPS_NUM          0x37

/* Legal values for DT_MIPS_FLAGS Elf32_Dyn entry.  */

#define RHF_NONE                   0            /* No flags */
#define RHF_QUICKSTART             (1 << 0)     /* Use quickstart */
#define RHF_NOTPOT                 (1 << 1)     /* Hash size not power of 2 */
#define RHF_NO_LIBRARY_REPLACEMENT (1 << 2)     /* Ignore LD_LIBRARY_PATH */
#define RHF_NO_MOVE                (1 << 3)
#define RHF_SGI_ONLY               (1 << 4)
#define RHF_GUARANTEE_INIT         (1 << 5)
#define RHF_DELTA_C_PLUS_PLUS      (1 << 6)
#define RHF_GUARANTEE_START_INIT   (1 << 7)
#define RHF_PIXIE                  (1 << 8)
#define RHF_DEFAULT_DELAY_LOAD     (1 << 9)
#define RHF_REQUICKSTART           (1 << 10)
#define RHF_REQUICKSTARTED         (1 << 11)
#define RHF_CORD                   (1 << 12)
#define RHF_NO_UNRES_UNDEF         (1 << 13)
#define RHF_RLD_ORDER_SAFE         (1 << 14)

/* Entries found in sections of type SHT_MIPS_LIBLIST.  */

typedef struct
{
  Elf32_Word l_name;            /* Name (string table index) */
  Elf32_Word l_time_stamp;      /* Timestamp */
  Elf32_Word l_checksum;        /* Checksum */
  Elf32_Word l_version;         /* Interface version */
  Elf32_Word l_flags;           /* Flags */
} Elf32_Lib;

typedef struct
{
  Elf64_Word l_name;            /* Name (string table index) */
  Elf64_Word l_time_stamp;      /* Timestamp */
  Elf64_Word l_checksum;        /* Checksum */
  Elf64_Word l_version;         /* Interface version */
  Elf64_Word l_flags;           /* Flags */
} Elf64_Lib;


/* Legal values for l_flags.  */

#define LL_NONE           0
#define LL_EXACT_MATCH    (1 << 0)      /* Require exact match */
#define LL_IGNORE_INT_VER (1 << 1)      /* Ignore interface version */
#define LL_REQUIRE_MINOR  (1 << 2)
#define LL_EXPORTS        (1 << 3)
#define LL_DELAY_LOAD     (1 << 4)
#define LL_DELTA          (1 << 5)

/* Entries found in sections of type SHT_MIPS_CONFLICT.  */

typedef Elf32_Addr Elf32_Conflict;

typedef struct
{
  /* Version of flags structure.  */
  Elf32_Half version;
  /* The level of the ISA: 1-5, 32, 64.  */
  unsigned char isa_level;
  /* The revision of ISA: 0 for MIPS V and below, 1-n otherwise.  */
  unsigned char isa_rev;
  /* The size of general purpose registers.  */


/*
 * These are the types of tokens returned by the scanner.  The first
 * three are also used in the hash table of capability names.  The scanner
 * returns one of these values after loading the specifics into the global
 * structure curr_token.
 */

#define BOOLEAN 0               /* Boolean capability */
#define NUMBER 1                /* Numeric capability */
#define STRING 2                /* String-valued capability */
#define CANCEL 3                /* Capability to be cancelled in following tc's */
#define NAMES  4                /* The names for a terminal type */
#define UNDEF  5                /* Undefined */

#define NO_PUSHBACK     -1      /* used in pushtype to indicate no pushback */

/*
 * The global structure in which the specific parts of a
 * scanned token are returned.
 */

struct token
{
        char    *tk_name;       /* name of capability */
        int     tk_valnumber;   /* value of capability (if a number) */
        char    *tk_valstring;  /* value of capability (if a string) */
};

/*
 * Offsets to string capabilities, with the corresponding functionkey codes.
 */
struct tinfo_fkeys {
        unsigned offset;
        chtype code;
        };

typedef short HashValue;

/*
 * The file comp_captab.c contains an array of these structures, one per
 * possible capability.  These are indexed by a hash table array of pointers to
 * the same structures for use by the parser.
 */
struct name_table_entry
{
        const char *nte_name;   /* name to hash on */
        int     nte_type;       /* BOOLEAN, NUMBER or STRING */
        HashValue nte_index;    /* index of associated variable in its array */
        HashValue nte_link;     /* index in table of next hash, or -1 */
};

/*
 * Use this structure to hide differences between terminfo and termcap tables.
 */
typedef struct {
        unsigned table_size;
        const HashValue *table_data;
        HashValue (*hash_of)(const char *);
        int (*compare_names)(const char *, const char *);
} HashData;

struct alias
{
        const char      *from;
        const char      *to;
        const char      *source;
};

#define NOTFOUND        ((struct name_table_entry *) 0)

#endif /* __TIC_H */
    }
    while(1)
    {
        printf("%s]# ",current_path);
        scanf("%s",command);
        char name[255];
        if(!strcmp(command,"cd"))//change dir
        {
            char path[256];
            scanf("%s",path);
            cd(path);
        }
        else if(!strcmp(command,"mkdir"))//create dir
        {
            scanf("%s",name);
            create(2,name);
        }
        else if(!strcmp(command,"mkf"))//create file
        {
            scanf("%s",name);
            create(1,name);
        }
        else if(!strcmp(command,"rmdir"))
        {
            scanf("%s",name);
            if(!strcmp(name,"-r"))//delete all in dir
            {
                char name_[255];
                scanf("%s",name_);
                delete(2,name_,0);
            }
            else 
            {
                delete(2,name,1);//delete empty dir
            }
        }
        else if(!strcmp(command,"rm"))//delete file
        {
            scanf("%s",name);
            delete(1,name,0);
        }
        else if(!strcmp(command,"open"))
        {
            scanf("%s",name);
            open_file(name);
        }
        else if(!strcmp(command,"close"))
        {
            scanf("%s",name);
            close_file(name);
        }
        else if(!strcmp(command,"read"))
        {
            scanf("%s",name);
            read_file(name);
        }
        else if(!strcmp(command,"write"))
        {
            scanf("%s",name);
            char flag[3];
            scanf("%s",flag);
            char source[2560];
            scanf("%s",source);
            if(!strcmp(flag,">"))
                write_file(name,0,source);
            else if(!strcmp(flag,">>"))
                write_file(name,1,source);
            else
                printf("Wrong command!\n"); 
        }
        else if(!strcmp(command,"ls"))//list items
            dir_ls();
        else if(!strcmp(command,"help"))//help menu
            help();
        else if(!strcmp(command,"format"))//format disk
        {
            format();
        }
        else if(!strcmp(command,"passwd"))
        {
            password();
        }
        else if(!strcmp(command,"login"))
        {
            printf("Failed! You haven't logged out yet.\n");
        }
        else if(!strcmp(command,"logout"))
        {
            logout();
        }
        else if(!strcmp(command,"chmod"))
        {
            scanf("%s",name);
            char rwx[6];
            scanf("%s",rwx);
            chmod(name,rwx);
        }
        else if(!strcmp(command,"quit"))//logout
        {
            quit();
            break;
        }
        else
            printf("No this Command.Please check!\n");
    }
}

int main()
{
    initialize_disk();
    initialize_memory();
    printf("Hello! Welcome to simulation of ext2 file system!\n");
    if(!login()) return 0;
    shell();
    return 0;
}
