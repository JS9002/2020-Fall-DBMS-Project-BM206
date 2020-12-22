#include <string> 
#include <fstream> 
#include <iostream> 
#include <time.h> 
#include "pml_hash.h"
using namespace std;

uint64_t convert(string str){
   uint64_t s = str[0] - '0';
   for(int i = 1 ;i< 8 ; i++){
       s *= 10;         //��ȡǰ���ֽ� 
       s += str[i] - '0';
   }
   return s;
}
   
int main(){     
	PMLHash hash("/home/zwz/test/pml_hash");
	uint64_t key  , value;
    string op , str;
	char file[10][50] ={"/home/zwz/test/data/10w-rw-0-100-load.txt",
					    "/home/zwz/test/data/10w-rw-0-100-run.txt" , 
						"/home/zwz/test/data/10w-rw-25-75-run.txt" ,
						"/home/zwz/test/data/10w-rw-50-50-run.txt" ,
						"/home/zwz/test/data/10w-rw-75-25-run.txt" ,
						"/home/zwz/test/data/10w-rw-100-0-run.txt" };
      					//benchmark���ݼ�����·��   
	for(int i = 1 ; i < 6 ; i++){

		ifstream read(file[0]);
   		if(!read.is_open()) {
        	cout << "load failed " << endl;
        	cin.get();
			return 0;
    	}	
        cout<<"Start loading("<<i<<")..."<<endl;
    	while(!read.eof()){ //��ȡload�ļ�	
			 
       		read >> op >> str;
       		key = convert(str);
		hash.insert(key , key);//key��valueĬ�����
		}

		cout<<"Load("<<i<<") successfully!"<<endl;
    	read.close();   

		ifstream read2(file[i]);
    	if(!read2.is_open()) {
      		cout << "run failed " << endl;
        	cin.get();
        	return 0; 
    	}
    	int t = 0 , insert_num = 0 , read_num = 0;
    	clock_t start , end;
    	double time = 0;
    	cout<<"Start running("<<i<<")..."<<endl;
    	while(!read2.eof()){
       		read2 >> op >> key;
      		t++;    
			    
			if(op[0]=='I') {   //����ȡ����INSERT������ 
	       		insert_num++;
				start = clock();
				hash.insert(key , key);
			}
			else {      //����Ϊ��SEARCH������ 
				read_num++;
				start = clock();
		    	hash.search(key , value);
			}
        	end = clock();
			time += end - start;//ͳ��ʱ�� 
			if(t==10000) break;  //eof��ԭ�����һ�� 
    	}
    	cout<<"Run("<<i<<") successfully!"<<endl;
		  
    	cout << "Operations size : " << t  << endl; //�������ָ�� 
    	cout << "Total time cost : " << time/1000  << " ms " << endl;//linux��Ĭ�ϵ�λ��us 
    	cout << "Insert_num      : " << insert_num << endl;  
    	cout << "Search_num      : " << t -  insert_num << endl;
		cout << "OPS             : " << t/(time/1000000) << endl;
		cout << "Avg_Latency     : " << time/t << " us " << endl;  
		cout << endl<< endl;
    	read2.close();
        cin.get();
		hash.destroy(); //��� 
	}

    return 0;
}
