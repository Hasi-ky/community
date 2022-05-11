#include<iostream>
#include<vector>
#include<string>
#include<algorithm>
#include<fstream>
#include<sstream>
#include<set>
#include<map>
#include<queue>
#include<numeric>
#include<math.h>
using namespace std;

struct bag {
	int index; //这个包的来源于哪一台主机的请求
	int val;  //这个包带有多少流量   
	string liu; //这个包来自于哪个流
	int toserver; //来源于哪些服务器
	bool used; //是否使用过
	friend bool operator < (const bag &a, const bag &b)
	{
		return a.val < b.val;
	}
};

struct time_Node {
	int index;
	int val;
};

struct server_Node {
	string name;
	int wei,index, used_val; //权重和容量的表示方法 contain表示余量
	bool used;//是否使用过

	vector<int> bags;  //统计放入了哪些流量方法的下标
};

//如果这个包到服务器的[index][j] qos是不通的，那么就是这个包无法存放
vector<vector<vector<vector<string>>>> result;  //每时刻所有客户结点使服务器，对应流对应的量
vector<vector<int>> demand;
vector<vector<int>> p2p;  //两个站点之间的连接时延
vector<vector<vector<int>>> ans;  //表示每个时刻每台主机使用的流编号
vector<string> zhuji;
vector<string> bianyuan;
vector<int> limit;  //表示每个边缘上限
map<string, int> demand_to_zhuji;   //读取demand中的主机顺序
map<string, int > server_to_qos;  // 以qos中的顺序为主
vector<vector<vector<int>>> time_to_need;  //每个时间段对应流的信息
vector<vector<string>> time_to_stream;    //每个时间点对应流的id
vector<vector<int>> time_to_sum; //记录每个时间段内每台主机所需要的总量
vector<int> weight;   //表示每台服务器对应的权重
vector<vector<int>> zhu_to_server;   //主机可以从这些服务点中收获
vector<vector<int>> server_to_zhu;   //服务点可以从这些主机中收获


vector<vector<vector<int>>> all_server_to_bag; //所有时间每个服务器下有哪些包(all_q中的顺序）

vector<priority_queue<int, vector<int>, greater<int>>> v_pq; //服务器的95%
vector<priority_queue<int, vector<int>, greater<int>>> v_pq2;
vector<bool> server_used; // 该服务器是否使用过


vector<vector<bag>> all_q1; //所有时间所有包的集合

vector<vector<int>> all_spend_money;//服务器每个时间花的钱

vector<vector<server_Node>> all_server_node; //每个时间的所有服务器

vector<vector<bool>> pre_server_used; //是否提前分配过

int upper;      //表征qos上限
int base;       //V值
int zhuji_sum;  //主机数量
int server_sum;  //服务器数量
int time_sum;   //时间总数

int N;


//服务器按容量排序
bool cmp_server(server_Node a, server_Node b) {
	return a.wei > b.wei;
}

int cmp_time(time_Node a, time_Node b) {
	return a.val > b.val;
}

int cmp_bag(bag a, bag b) {
	return a.val > b.val;
}

//配置文件参数读取
#pragma region config
//定义模板类对象
class RrConfig
{
public:
	RrConfig()
	{
	}
	~RrConfig()
	{
	}
	bool ReadConfig(const std::string& filename);
	std::string ReadString(const char* section, const char* item, const char* default_value);
	int ReadInt(const char* section, const char* item, const int& default_value);
	//float ReadFloat(const char* section, const char* item, const float& default_value);
private:
	bool IsSpace(char c);
	bool IsCommentChar(char c);
	void Trim(std::string& str);
	bool AnalyseLine(const std::string& line, std::string& section, std::string& key, std::string& value);

private:
	std::map<std::string, std::map<std::string, std::string> >settings_;
};

bool RrConfig::IsSpace(char c)
{
	if (' ' == c || '\t' == c)
		return true;
	return false;
}

bool RrConfig::IsCommentChar(char c)
{
	switch (c) {
	case '#':
		return true;
	default:
		return false;
	}
}

void RrConfig::Trim(std::string& str)
{
	if (str.empty())
	{
		return;
	}
	int i, start_pos, end_pos;
	for (i = 0; i < str.size(); ++i) {
		if (!IsSpace(str[i])) {
			break;
		}
	}
	if (i == str.size())
	{
		str = "";
		return;
	}
	start_pos = i;
	for (i = str.size() - 1; i >= 0; --i) {
		if (!IsSpace(str[i])) {
			break;
		}
	}
	end_pos = i;
	str = str.substr(start_pos, end_pos - start_pos + 1);
}

bool RrConfig::AnalyseLine(const std::string& line, std::string& section, std::string& key, std::string& value)
{
	if (line.empty())
		return false;
	int start_pos = 0, end_pos = line.size() - 1, pos, s_startpos, s_endpos;
	if ((pos = line.find("#")) != -1)
	{
		if (0 == pos)
		{
			return false;
		}
		end_pos = pos - 1;
	}
	if (((s_startpos = line.find("[")) != -1) && ((s_endpos = line.find("]"))) != -1)
	{
		section = line.substr(s_startpos + 1, s_endpos - 1);
		return true;
	}
	std::string new_line = line.substr(start_pos, start_pos + 1 - end_pos);
	if ((pos = new_line.find('=')) == -1)
		return false;
	key = new_line.substr(0, pos);
	value = new_line.substr(pos + 1, end_pos + 1 - (pos + 1));
	Trim(key);
	if (key.empty()) {
		return false;
	}
	Trim(value);
	if ((pos = value.find("\r")) > 0)
	{
		value.replace(pos, 1, "");
	}
	if ((pos = value.find("\n")) > 0)
	{
		value.replace(pos, 1, "");
	}
	return true;
}

bool RrConfig::ReadConfig(const std::string& filename)
{
	settings_.clear();
	std::ifstream infile(filename.c_str());
	if (!infile) {
		return false;
	}
	std::string line, key, value, section;
	std::map<std::string, std::string> k_v;
	std::map<std::string, std::map<std::string, std::string> >::iterator it;
	while (getline(infile, line))
	{
		if (AnalyseLine(line, section, key, value))
		{
			it = settings_.find(section);
			if (it != settings_.end())
			{
				k_v[key] = value;
				it->second = k_v;
			}
			else
			{
				k_v.clear();
				settings_.insert(std::make_pair(section, k_v));
			}
		}
		key.clear();
		value.clear();
	}
	infile.close();
	return true;
}

std::string RrConfig::ReadString(const char* section, const char* item, const char* default_value)
{
	std::string tmp_s(section);
	std::string tmp_i(item);
	std::string def(default_value);
	std::map<std::string, std::string> k_v;
	std::map<std::string, std::string>::iterator it_item;
	std::map<std::string, std::map<std::string, std::string> >::iterator it;
	it = settings_.find(tmp_s);
	if (it == settings_.end())
	{
		return def;
	}
	k_v = it->second;
	it_item = k_v.find(tmp_i);
	if (it_item == k_v.end())
	{
		return def;
	}
	return it_item->second;
}

int RrConfig::ReadInt(const char* section, const char* item, const int& default_value)
{
	std::string tmp_s(section);
	std::string tmp_i(item);
	std::map<std::string, std::string> k_v;
	std::map<std::string, std::string>::iterator it_item;
	std::map<std::string, std::map<std::string, std::string> >::iterator it;
	it = settings_.find(tmp_s);
	if (it == settings_.end())
	{
		return default_value;
	}
	k_v = it->second;
	it_item = k_v.find(tmp_i);
	if (it_item == k_v.end())
	{
		return default_value;
	}
	return atoi(it_item->second.c_str());
}

#pragma endregion
// 获取参数
#pragma region get
//获取demand文件
void getNum(string line, ifstream& tin) {
	bool isStart = true;
	int len = 0;
	string pre = ""; //表示为上一次的时间结点
	int ceng = 0;
	vector<vector<int>> temp;
	vector<string> stream_id;//
	while (getline(tin, line))
	{
		istringstream tin(line);
		string field;
		int isBegin = 0;
		if (isStart) {
			int index = 0;  //标记主机
			if (zhuji.empty()) {
				while (getline(tin, field, ',')) {
					if (isBegin < 2) { //掐除stream_id
						isBegin++;
						continue;
					}
					else {
						zhuji.push_back(field);   //这里首先获得的是demand中的主机顺序
						demand_to_zhuji[field] = index++;  //标记map
					}
				}
			}
			isStart = false;

		}
		else {
			isBegin = 0;
			vector<int> tt;
			while (getline(tin, field, ',')) {
				if (isBegin < 2) {
					if (isBegin == 0) {
						if (pre != field) {
							if (!temp.empty()) {
								time_to_need.push_back(temp);
								time_to_stream.push_back(stream_id);
								int sz = temp.size();
								vector<int> res((int)temp[0].size(), 0);
								for (int i = 0; i < sz; i++) {
									for (int j = 0; j < temp[i].size(); j++) {
										res[j] += temp[i][j];
									}
								}
								time_to_sum.push_back(res);
							}
							pre = field;
							temp.clear();//重启
							stream_id.clear(); //重启
						}
					}
					else {
						stream_id.push_back(field);
					}
					isBegin++;
					continue;
				}
				else {
					tt.push_back(stoi(field));  //
				}
			}
			temp.push_back(tt);
		}
	}
	if (!stream_id.empty()) {
		time_to_need.push_back(temp);
		time_to_stream.push_back(stream_id);
		int sz = temp.size();
		vector<int> res((int)temp[0].size(), 0);
		for (int i = 0; i < sz; i++) {
			for (int j = 0; j < temp[i].size(); j++) {
				res[j] += temp[i][j];
			}
		}
		time_to_sum.push_back(res);
	}
}

//获取qos文件
void getQos(string line, ifstream& tin) {
	bool isStart = true;
	vector<string> temp_zhuji;  //qos中主机的顺序
	int len = 0;
	int num = 0;  //表示服务器编号
	while (getline(tin, line))
	{
		istringstream tin(line);
		string field;
		bool isBegin = true;
		if (isStart) {  //头一行
			while (getline(tin, field, ',')) {
				if (isBegin) {  //site_name
					isBegin = false;
					continue;
				}
				else {
					temp_zhuji.push_back(field);//首先拿到对应qos当中对应主机的顺序
				}
			}
			isStart = false;
			continue;
		}
		vector<int> temp(temp_zhuji.size(), 0);
		int count = 0;
		while (getline(tin, field, ',')) {
			if (isBegin) {
				server_to_qos[field] = num++;  //记录哪个服务器对应哪个边
				bianyuan.push_back(field); //边缘服务器的顺序,
				isBegin = false;
				continue;
			}
			else {
				int pix = demand_to_zhuji[temp_zhuji[count]];
				temp[pix] = stoi(field);
				count++;
			}
		}
		p2p.push_back(temp);
	}
}

//获取上限
void getLimit(string line, ifstream& kin) {
	limit.resize(server_to_qos.size());
	bool isStart = true;
	while (getline(kin, line)) {
		istringstream kin(line); //将整行字符串line读入到字符串流istringstream中
		string field;
		if (isStart) {
			isStart = false;
			continue;
		}
		else {
			int count = 0;
			string s1 = "";
			while (getline(kin, field, ',')) {//将字符串流sin中的字符读入到field字符串中，以逗号为分隔符
				if (count & 1) {
					int index = server_to_qos[s1];
					limit[index] = (stoi(field));
				}
				else {
					s1 = field;
				}
				count++;
			}
		}
	}
}
#pragma endregion 

void init() {
	string line;
	ifstream fin("../../data/demand.csv"); //打开文件流操作
	getNum(line, fin);  //获得demand相关信息
	ifstream kin("../../data/qos.csv"); //打开文件流操作
	getQos(line, kin);  //获得demand相关信息
	ifstream tin("../../data/site_bandwidth.csv");
	getLimit(line, tin);

	RrConfig config;
	config.ReadConfig("../../data/config.ini");
	upper = config.ReadInt("config", "qos_constraint", 0);
	base = config.ReadInt("config", "base_cost", 0);
	time_sum = time_to_stream.size();
	zhuji_sum = zhuji.size();
	server_sum = server_to_qos.size();
	weight.resize(server_sum);
	fin.close();
	kin.close();
	tin.close();
}

void init2() {
	//双边初始化
	zhu_to_server.resize(zhuji_sum);
	server_to_zhu.resize(server_sum);
	for (int i = 0; i < server_sum; i++) {
		for (int j = 0; j < zhuji_sum; j++) {
			if (p2p[i][j] < upper) {
				weight[i] ++;
				zhu_to_server[j].push_back(i);
				server_to_zhu[i].push_back(j);
			}
		}
	}
	//排序
	for (int i = 0; i < zhuji_sum; i++) {
		sort(zhu_to_server[i].rbegin(), zhu_to_server[i].rend());
	}
	//容量初始化
	v_pq = vector <priority_queue<int, vector<int>, greater<int>>>(server_sum, priority_queue<int, vector<int>, greater<int>>());
	v_pq2 = vector <priority_queue<int, vector<int>, greater<int>>>(server_sum, priority_queue<int, vector<int>, greater<int>>());
	server_used = vector<bool> (server_sum, false);
	for (int i = 0; i < server_sum; i++) {
		for (int j = 0; j < N; j++) {
			v_pq[i].push(0);
			if (j < N - 1) v_pq2[i].push(0);
		}
	}
	//显示初始化
	all_spend_money = vector<vector<int>>(server_sum, vector<int>(time_sum, 0));
	
	//所有包的加入 和 服务器下包的加入
	all_q1 = vector<vector<bag>>(time_sum, vector<bag>());
	all_server_to_bag = vector<vector<vector<int>>>(time_sum , vector<vector<int>>(server_sum, vector<int>()));
	for (int i = 0; i < time_sum; i++) {
		for (int j = 0; j < time_to_need[i].size(); j++) {
			for (int k = 0; k < zhuji_sum; k++) {
				bag b1; b1.index = k; b1.liu = time_to_stream[i][j]; b1.val = time_to_need[i][j][k]; b1.toserver = -1;
				all_q1[i].push_back(b1);
			}
		}
		sort(all_q1[i].begin(), all_q1[i].end(), cmp_bag);
	}
	for (int i = 0; i < time_sum; i++) {
		for (int j = 0; j < all_q1[i].size(); j++) {
			for (int z = 0; z < zhu_to_server[all_q1[i][j].index].size(); z++) {
				int cur_server = zhu_to_server[all_q1[i][j].index][z];
				all_server_to_bag[i][cur_server].push_back(j);
			}
		}
	}
	//result初始化
	result = vector<vector<vector<vector<string>>>>(time_sum, vector<vector<vector<string>>>(zhuji_sum, vector<vector<string>>(server_sum, vector<string>())));
	//服务器初始化
	all_server_node = vector<vector<server_Node>>(time_sum, vector<server_Node>());
	for (int i = 0; i < time_sum; i++) {
		for (int j = 0; j < server_sum; j++) {
			server_Node tmp_node;
			tmp_node.index = j;
			tmp_node.used_val = 0;
			all_server_node[i].push_back(tmp_node);
		}
	}
	
	pre_server_used = vector<vector<bool>>(time_sum, vector<bool>(server_sum, false));
}

//预处理 提前分配前百分之5 服务器不一定全部使用： 所以可以设定热点服务器，非热点服务器不进行提前分配（后续优化）
void pre_distribute() {
	//服务器排序 容量从大到小
	vector<server_Node> v_server;
	for (int i = 0; i < server_sum; i++) {
		//对于小数值的server不进行预处理（可优化）
		if (limit[i] < base * 2) continue;
		server_Node tmp_node;
		tmp_node.index = i;
		tmp_node.wei = limit[i];
		v_server.push_back(tmp_node);
	}
	sort(v_server.begin(), v_server.end(), cmp_server);

	//对于每个服务器取百分之五的时间
	for (int i = 0; i < v_server.size(); i++) {
		int cur_server = v_server[i].index;
		vector<time_Node> v_time;
		for (int j = 0; j < time_sum; j++) {
			int cur_time = j;
			//时间排序
			time_Node tmp_node;
			tmp_node.index = cur_time;
			tmp_node.val = 0;
			for (int z = 0; z < all_server_to_bag[cur_time][cur_server].size(); z++) {
				int cur_bag = all_server_to_bag[cur_time][cur_server][z];
				if (all_q1[cur_time][cur_bag].used) continue;
				tmp_node.val += all_q1[cur_time][cur_bag].val;
			}
			v_time.push_back(tmp_node);
		}
		sort(v_time.begin(), v_time.end(), cmp_time);

		for (int j = 0; j < N - 1; j++) {
			int cur_time = v_time[j].index;
			//cur_server对cur_time下所有包进行吸收
			for (int z = 0; z < all_server_to_bag[cur_time][cur_server].size(); z++) {
				int cur_bag = all_server_to_bag[cur_time][cur_server][z];
				if (all_q1[cur_time][cur_bag].toserver != -1) continue;
				if (all_q1[cur_time][cur_bag].val + all_server_node[cur_time][cur_server].used_val <= limit[cur_server]) {
					all_q1[cur_time][cur_bag].toserver = cur_server;
					all_server_node[cur_time][cur_server].used_val += all_q1[cur_time][cur_bag].val;
					all_server_node[cur_time][cur_server].bags.push_back(cur_bag);
				}
				if (all_server_node[cur_time][cur_server].used_val == limit[cur_server]) break;
			}
			//优化 没吸收完就检索被其他服务器占用的其他包 可以通过 + 100提高速度
			/*if (all_server_node[cur_time][cur_server].used_val < limit[cur_server]) {
				for (int z = 0; z < all_server_to_bag[cur_time][cur_server].size(); z++) {
					if (all_server_to_bag[cur_time][cur_server][z].toserver != cur_server) {
						int last_server = all_server_to_bag[cur_time][cur_server][z].toserver;
						//上个服务器最多可以给多少 < 当前服务器空出来的量     上个服务器 + 吸收量 - 给的量 < limit
						//给的量 > 吸收量 + 本身的差  < 空出量

					}
				}
			}*/
			
			//优先队列
			if (all_server_node[cur_time][cur_server].used_val > v_pq[cur_server].top()) {
				v_pq[cur_server].pop();
				v_pq[cur_server].push(all_server_node[cur_time][cur_server].used_val);
			}
			if (all_server_node[cur_time][cur_server].used_val > v_pq2[cur_server].top()) {
				v_pq2[cur_server].pop();
				v_pq2[cur_server].push(all_server_node[cur_time][cur_server].used_val);
			}
			if (all_server_node[cur_time][cur_server].used_val > 0) {
				server_used[cur_server] = true;
				pre_server_used[cur_time][cur_server] = true;
			}
		}
	}
}



void free(int cur_time) {
	/*for (int i = 0; i < server_sum; i++) {
		int cur_server = i;
		if (all_server_node[cur_time][cur_server].used_val < v_pq[cur_server].top()) {
			for (int j = 0; j < all_server_to_bag[cur_time][cur_server].size(); j++) {
				int cur_bag = all_server_to_bag[cur_time][cur_server][j];
				if (all_q1[cur_time][cur_bag].toserver != -1) continue;
				if (all_server_node[cur_time][cur_server].used_val + all_q1[cur_time][cur_bag].val <= v_pq[cur_server].top()) {
					all_server_node[cur_time][cur_server].used_val += all_q1[cur_time][cur_bag].val;
					all_q1[cur_time][cur_bag].toserver = cur_server;
					all_server_node[cur_time][cur_server].bags.push_back(cur_bag);
				}
				if (all_server_node[cur_time][cur_server].used_val + all_q1[cur_time][cur_bag].val == v_pq[cur_server].top()) break;
			}
		}
	}*/
	for (int i = 0; i < all_q1[cur_time].size(); i++) {
		if (all_q1[cur_time][i].toserver != -1) continue;
		for (int j : zhu_to_server[all_q1[cur_time][i].index]) { //j表示主机的服务器
			int cur_server = j;
			if (v_pq[cur_server].top() - all_server_node[cur_time][j].used_val >= all_q1[cur_time][i].val) {
				all_server_node[cur_time][j].used_val += all_q1[cur_time][i].val;
				all_server_node[cur_time][j].bags.push_back(i);
				all_q1[cur_time][i].toserver = j;
				break;
			}
		}
	}
}

void distribute(vector<int> &cur_limit, int cur_time) {

	//预分配一定分配不下的包
	for (int i = 0; i < all_q1[cur_time].size(); i++) {
		if (all_q1[cur_time][i].toserver != -1) continue;
		bool tmp_flag = false;
		for (int j : zhu_to_server[all_q1[cur_time][i].index]) {
			if (cur_limit[j] - all_server_node[cur_time][j].used_val >= all_q1[cur_time][i].val) {
				tmp_flag = true;
				break;
			}
		}
		//分配不下
		if (!tmp_flag) {
			
			int max_index = -1;
			int max_value = -100000000;
			for (int z : zhu_to_server[all_q1[cur_time][i].index]) {
				//此处为总限制
				if (limit[z] - all_server_node[cur_time][z].used_val >= all_q1[cur_time][i].val && cur_limit[z] - all_server_node[cur_time][z].used_val > max_value) {
					max_value = cur_limit[z] - all_server_node[cur_time][z].used_val;
					max_index = z;
				}
			}
			//分配
			if (max_index != -1) {
				all_server_node[cur_time][max_index].used_val += all_q1[cur_time][i].val;
				all_server_node[cur_time][max_index].bags.push_back(i);
				all_q1[cur_time][i].toserver = max_index;
			}
		}
	}

	free(cur_time);

	//先直接分
	for (int i = 0; i < all_q1[cur_time].size(); i++) {
		if (all_q1[cur_time][i].toserver != -1) continue;
		for (int j : zhu_to_server[all_q1[cur_time][i].index]) { //j表示主机的服务器
			if (cur_limit[j] - all_server_node[cur_time][j].used_val >= all_q1[cur_time][i].val) {
				all_server_node[cur_time][j].used_val += all_q1[cur_time][i].val;
				all_server_node[cur_time][j].bags.push_back(i);
				all_q1[cur_time][i].toserver = j;
				break;
			}
		}
	}

	//剩下的所有部分 选最小的(x - v) / y
	for (int i = 0; i < all_q1[cur_time].size(); i++) {
		if (all_q1[cur_time][i].toserver != -1) continue;
		double min_value = 1;
		int min_index = -1;
		for (int j : zhu_to_server[all_q1[cur_time][i].index]) {
			if (limit[j] - all_server_node[cur_time][j].used_val >= all_q1[cur_time][i].val && ((double) (all_server_node[cur_time][j].used_val + all_q1[cur_time][i].val) / (double) limit[j]) < min_value) {
				min_value = (double)(all_server_node[cur_time][j].used_val + all_q1[cur_time][i].val) / (double)limit[j];
				min_index = j;
			}
		}
		if (min_index != -1) {
			all_server_node[cur_time][min_index].used_val += all_q1[cur_time][i].val;
			all_server_node[cur_time][min_index].bags.push_back(i);
			all_q1[cur_time][i].toserver = min_index;
		}

	}

}



//某一时刻，在服务器限制limit下进行尽可能分配， 分配不成功也会退出
void fin_distribute(int cur_time) {
	

	vector<int> tmp_limit; //自定义限制
	for (int j = 0; j < server_sum; j++) {
		tmp_limit.push_back(base + limit[j] / 20);
	}

	distribute(tmp_limit, cur_time);
	


	for (auto j : all_server_node[cur_time]) {
		for (int s : j.bags) {
			result[cur_time][all_q1[cur_time][s].index][j.index].push_back(all_q1[cur_time][s].liu);
		}
	}

	for (int j = 0; j < server_sum; j++) {
		//记录此次时间服务器的价格
		all_spend_money[j][cur_time] = all_server_node[cur_time][j].used_val;

		//优先队列
		if (all_server_node[cur_time][j].used_val > v_pq[j].top() && !pre_server_used[cur_time][j]) {
			v_pq[j].pop();
			v_pq[j].push(all_server_node[cur_time][j].used_val);
		}
		if (all_server_node[cur_time][j].used_val > v_pq2[j].top() && !pre_server_used[cur_time][j]) {
			v_pq2[j].pop();
			v_pq2[j].push(all_server_node[cur_time][j].used_val);
		}
		if (all_server_node[cur_time][j].used_val > 0) server_used[j] = true;
	}
	
}



int main() {
	remove("../../output/solution.txt");
	
	init();
	N = time_sum / 20 + 1; //指第95%个
	init2();

	int len = zhuji[zhuji.size() - 1].size() - 1;
	if (zhuji[zhuji.size() - 1][len] == '\r') {
		zhuji[zhuji.size() - 1] = zhuji[zhuji.size() - 1].substr(0, len);
	}

	//后续使用
	//pre_distribute();

	//时间排序
	vector<time_Node> v_time;
	for (int i = 0; i < time_sum; i++) {
		time_Node tmp_node;
		tmp_node.index = i;
		tmp_node.val = 0;
		for (int j = 0; j < all_q1[i].size(); j++) {
			if (all_q1[i][j].used == false) {
				tmp_node.val += all_q1[i][j].val;
			}
		}
		v_time.push_back(tmp_node);
	}
	sort(v_time.begin(), v_time.end(), cmp_time);
	
	//进行分配
	for (int i = 0; i < time_sum; i++) {
		int cur_time = v_time[i].index;
		fin_distribute(cur_time);
	}

	//排序并显示
	for (int i = 0; i < server_sum; i++) {
		sort(all_spend_money[i].begin(), all_spend_money[i].end());
		cout << bianyuan[i] << " : ";
		for (int j = 0; j < all_spend_money[i].size(); j++) {
			cout << all_spend_money[i][j] << "、";
		}
		cout << endl;
		cout << endl;
	}


	int spend_money = 0;
	for (int i = 0; i < server_sum; i++) {
		if (!server_used[i]) continue;
		else if(v_pq[i].top() <= base) {
			spend_money += base;
		}
		else {
			spend_money += ((double)(v_pq[i].top() - base) * (double)(v_pq[i].top() - base) / (double)limit[i] + 0.5f) + v_pq[i].top();
		}
	}
	cout << spend_money << endl;
	ofstream ofs("../../output/solution.txt", ios::out | ios::app);
	for (int i = 0; i < time_sum; i++) {
		for (int j = 0; j < zhuji_sum; j++) {
			int flag = 0;
			ofs << zhuji[j] << ":";
			for (int k = 0; k < result[i][j].size(); k++) {
				if (result[i][j][k].empty()) continue;
				if (flag > 0) ofs << ",";
				ofs << "<" << bianyuan[k];
				for (int s = 0; s < result[i][j][k].size(); s++) {
					ofs << "," << result[i][j][k][s];
				}
				ofs << ">";
				flag++;
			}
			if (i != time_sum - 1 || j != zhuji_sum - 1)
				ofs << '\n';
		}
	}
	ofs.close();
	return 0;
}

