#include "pml_hash.h"

//��ʼ����ϣ���½�/���ǣ� 
PMLHash::PMLHash(const char* file_path) {
	size_t mapped_len;
	int is_pmem;
	if((start_addr = pmem_map_file(file_path, FILE_SIZE, PMEM_FILE_CREATE,
		0666, &mapped_len, &is_pmem)) == NULL){
		exit(0);
	}
	meta = (metadata*)start_addr;
    table = (pm_table*)((char*)start_addr + sizeof(meta));
    
    //��ʼ�����ݽṹ 
	if(meta->size == 0){
		meta->size = 1;
		meta->level = 0;
		meta->overflow_num = 0;
		meta->next = 1;

		for(int i = 0 ; i < MAX_IDX + 2 ; i++ ){
			table[i].fill_num = 0;
			table[i].next_offset = 0; 
		}
	}
}

//�ر��ļ� 
PMLHash::~PMLHash() {
	pmem_unmap(start_addr, FILE_SIZE);
}

//����Ͱ 
void PMLHash::split() {
	uint64_t index = meta->next;
	uint64_t newt = meta->next + (1<<(meta->level)) ;

	uint64_t i , j , k , tmp , tmp2;
	tmp = index;
	tmp2 = index;
	j = 0;
	k = 0;
	while(tmp2){        //��tmp2��i����Ҫ���ѵ�Ͱ
		for(i = 0 ; i < table[tmp2].fill_num ; i++ ){
			//��tmp��jȷ�����ھ�Ͱ�����ݵ�λ��
			if(table[tmp2].kv_arr[i].key % (1<<(meta->level+1)) + 1 == index){
				table[tmp].kv_arr[j].key = table[tmp2].kv_arr[i].key;
				table[tmp].kv_arr[j].value = table[tmp2].kv_arr[i].value;
				if(++j == TABLE_SIZE){
					table[tmp].fill_num = TABLE_SIZE;
					tmp = table[tmp].next_offset;
					j = 0;
				}
			}
			else{   //��newt��kȷ��������Ͱ�����ݵ�λ��
				if(k == TABLE_SIZE){
					table[newt].fill_num = TABLE_SIZE;
					newt = newOverflowIndex();
					k = 0;
				}
				table[newt].kv_arr[k].key = table[tmp2].kv_arr[i].key;
				table[newt].kv_arr[k].value = table[tmp2].kv_arr[i].value;
				k++;
			}
		}
		tmp2 = table[tmp2].next_offset;
	}
	
	recycle(table[tmp].next_offset); //�������Ͱ 
	table[tmp].fill_num = j;
	table[tmp].next_offset = 0;
	table[newt].fill_num = k;   //�޸Ĵ������ݵ�Ͱ����Ϣ 
	meta->size++;
	if(meta->next == (1<<(meta->level))){
		meta->next = 1;
		meta->level++;   //ԭͰȫ�����ѣ��޸���Ⱥ�next 
	}
	else meta->next++;
	pmem_persist(start_addr, FILE_SIZE);
}

//����ԭͰ�±� 
uint64_t PMLHash::hashFunc(const uint64_t &key ) {
	uint64_t index = key % (1<< meta->level) + 1;
	if(index < meta->next)
		index =  key % (1<< (meta->level+1)) + 1;
	return index; 
}


//����һ�����Ͱ���������±� 
uint64_t PMLHash::newOverflowIndex(){
	uint64_t recycle_head = MAX_IDX + 1;   //����Ͱ��ͷ�� 
	uint64_t rh = recycle_head;
    uint64_t index;
	if(table[rh].next_offset){         //�п��õĻ���Ͱ 
		index = table[rh].next_offset;
		table[rh].next_offset = table[index].next_offset;
	}
	else{
		index = MAX_IDX - (meta->overflow_num); //����������һ������Ͱ�����Ͱ 
		if(index < (meta->size)) index = 0;  //û�п��õ����Ͱ 
	}
	return index; 
}

void PMLHash::recycle(uint64_t index){
	uint64_t tmp = table[MAX_IDX + 1].next_offset;
	while(index){         //����һ���������Ͱ 
		table[index].fill_num = 0;      //������Ͱ��������Ϊ��Ч״̬ 
		table[index].next_offset = tmp;
		table[MAX_IDX + 1].next_offset = index; //�������Ͱ��ͷ
		index = table[index].next_offset;
		meta->overflow_num--;
	}
	pmem_persist(start_addr, FILE_SIZE);
}

//�����ֵ�� 
int PMLHash::insert(const uint64_t &key, const uint64_t &value) {
	uint64_t index = hashFunc(key);
	uint64_t tmp = index;
	
	while(table[tmp].fill_num == TABLE_SIZE){ //�ҵ��յ�Ͱ 
		if(table[tmp].next_offset == 0)
			table[tmp].next_offset = newOverflowIndex();
		tmp= table[tmp].next_offset;
		if(tmp == 0)     //û�п�������ռ䣬����ʧ�� 
			return -1;
	}
	
	table[tmp].kv_arr[table[tmp].fill_num].key = key;
	table[tmp].kv_arr[table[tmp].fill_num].value = value;
	table[tmp].fill_num++;
	if(tmp != index)
		split();
	pmem_persist(start_addr, FILE_SIZE);
	return 0;
}

//����key������Ӧ��value 
int PMLHash::search(const uint64_t &key, uint64_t &value) {
	uint64_t tmp = hashFunc(key);
	while(tmp){  //����ԭͰ�����������Ͱ 
		for(int i = 0 ; i < table[tmp].fill_num ; i++ ){
			if(table[tmp].kv_arr[i].key == key){
				value = table[tmp].kv_arr[i].value;
				return 0;
			}
		}
		tmp = table[tmp].next_offset;
	}
	return -1;
}

//����keyɾ��ĳһ��ֵ�� 
int PMLHash::remove(const uint64_t &key) {
	uint64_t index = hashFunc(key);
	uint64_t tmp = index;
	uint64_t tmp2 = index;

	while(table[tmp].next_offset){
		tmp2 = tmp;         //tmp2 index��Ӧ�ġ������ڶ�����Ͱ������ֻ��һ��Ͱ�� 
		tmp = table[tmp].next_offset;   //tmp���һ��Ͱ     
	}
	
	while(index){     
		for(int i = 0 ; i < table[index].fill_num ; i++ ){
			if(table[index].kv_arr[i].key == key){
				table[tmp].fill_num--;   //ɾ����λ����Ҫ�ɡ����һ�����Ͱ�������һ��Ԫ������� 
				table[index].kv_arr[i].value = table[tmp].kv_arr[table[tmp].fill_num].value;
				table[index].kv_arr[i].key = table[tmp].kv_arr[table[tmp].fill_num].key;       

				if((table[tmp].fill_num == 0) && (tmp != index)){
					table[tmp2].next_offset = 0;
					recycle(tmp);    //���տյ����Ͱ
				}
				pmem_persist(start_addr, FILE_SIZE);
				return 0;
			}
		}
		index = table[index].next_offset;
	}
	pmem_persist(start_addr, FILE_SIZE);
	return -1;
}

//����ĳ��key��Ӧ��value 
int PMLHash::update(const uint64_t &key, const uint64_t &value) {
	uint64_t tmp = hashFunc(key);  
	while(tmp){   //�������� 
		for(int i = 0 ; i < table[tmp].fill_num ; i++){
			if(table[tmp].kv_arr[i].key == key){
				table[tmp].kv_arr[i].value = value;
				pmem_persist(start_addr, FILE_SIZE);
				return 0;
			}
		}
		tmp = table[tmp].next_offset;
	}
	pmem_persist(start_addr, FILE_SIZE);
	return -1; 
}

//�������Ͱ 
void PMLHash::print(){
	uint64_t tmp;
	int i , j;
	for(i = 1 ; i <= meta->size ; i++){
		tmp = i;
		j = 0;
		while(tmp){
			if(j == 0) cout << " table " << i << " :  " ;
				else cout << " ----overflow : " ;
			for(int k = 0 ; k < table[tmp].fill_num ; k++){
				cout << "( " << table[tmp].kv_arr[k].key << " , "<<table[tmp].kv_arr[k].value << ") ";  
			}
			cout << endl;
			tmp = table[tmp].next_offset;
			j++;
		}
		cout << endl << endl;
	}
}
