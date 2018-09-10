#include "predict.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <float.h>

using namespace std;

int daycnt(string, string);
void getphyinfo(char ** , vector<int> &);
void getflainfo(char ** , map<string, vector<int>> &);
void getDepPriorInfo(char ** , string &);
void getinfo(char **, map<string, vector<int>> &, string &, string &);
void getdata(char **, int, map<string, vector<int>> &, string &);
void predict(map<string, vector<int>> &, map<string, int> &, int);
void deploy(const vector<int> &, const map<string, vector<int>> &, const map<string, int> &, map<int, map<string, int>> &, string &);
char* getResFile(map<string, int> &, map<int, map<string, int>> &);

// info - Phy server info & vitual server info
// data - Training data
void predict_server(char * info[MAX_INFO_NUM], char * data[MAX_DATA_NUM], int data_num, char * filename){	
	// step_0 data struct initial
	map<string, vector<int>> vtseries;	// vtseries - {"flavor3",[0]...[N]} // history flavor used data, prepare this for predict
	map<string, int> vpred;				// vpred - {"flavor3",5} // prediction flavor used data, result of predict process, prepare this for deploy them on physical machine
	string tdatebeg(""), tdateend("");	// "2017-01-07" // predition date range
	vector<int> phyInfo(2, 0);			// phyInfo[0] - CPU , phyInfo[1] - MEM // physical machine infomation
	map<string, vector<int>> flaInfo;	// flaInfo - {"flavor3",[0] - CPU, [1] - MEM} // flavor infomation
	string dep_prior;					// dep_prior = "CPU" or "MEM" // deployment strategy should consider use cpu more effectly or mem more effectly;
	map<int, map<string, int>> dep;		// dep - {phyMacId, "flavor1",2 , "falvor2", 5} // deployment result 

	// step_1 get infoMes
	getphyinfo(info, phyInfo);						// get phyinfo
	getflainfo(info, flaInfo);						// get flainfo
	getDepPriorInfo(info, dep_prior);				// get dep_prior
	getinfo(info, vtseries, tdatebeg, tdateend);	// get vtseries - "flavor3" string part , get tdatebeg, tdateend
	getdata(data, data_num, vtseries, tdatebeg);	// get vtseries - "flavor3",[0]...[N] vector part
	
	// step_2 training and predict
	predict(vtseries, vpred, daycnt(tdatebeg, tdateend));		// prdict function

	// step_3 deploy the vistual machine to physical machine
	deploy(phyInfo, flaInfo, vpred, dep, dep_prior);

	// step_4 trans the asset policy from data to string
	char * result_file = getResFile(vpred, dep);	// save deployment infomation into char* result data structure

	// step_5 save the result into the file
	write_result(result_file, filename);	// save result into output file;

	return;
}

/***********************************************
 Support Part
***********************************************/
void getphyinfo(char ** info, vector<int> &phyinfo) {
	string cpuStr(""), memStr("");
	int cpuInt = 0, memInt = 0;
	char* it = *info;
	while (*it != ' ' && *it != '\n') cpuStr.push_back(*it++);
	it++;
	while (*it != ' ' && *it != '\n') memStr.push_back(*it++);
	cpuInt = stoi(cpuStr);
	memInt = stoi(memStr);
	phyinfo[0] = cpuInt;
	phyinfo[1] = memInt * 1024;
	return;
}
void getflainfo(char ** info, map<string, vector<int>> &flainfo) {
	string pstr("flavor");
	vector<int> vec;
	int i = 0;
	// get flavor info 
	while (*info[i] != 'M' && *info[i] != 'C') {
		vector<int> vline(2, 0);
		int cpuInt, memInt;
		string cpuStr, memStr, name;
		int j = 0;
		for (; *(info[i] + j) != ' ' && *(info[i] + j) != '\n'; j++) {
			name.push_back(*(info[i] + j));
		}
		if (name.compare(0, 6, pstr) == 0) {
			j++;
			while (*(info[i] + j) != ' ' && *(info[i] + j) != '\n') cpuStr.push_back(*(info[i] + j++));
			j++;
			while (*(info[i] + j) != ' ' && *(info[i] + j) != '\n') memStr.push_back(*(info[i] + j++));
			cpuInt = stoi(cpuStr);
			memInt = stoi(memStr);
			vline[0] = cpuInt;
			vline[1] = memInt;
			flainfo[name] = vline;
		}
		i++;
	}
	return;
}
void getDepPriorInfo(char ** info, string &dep_prior) {
	char ** it = info;
	while (**it != 'C' && **it != 'M') 
		it++;
	if (**it == 'C') dep_prior = "CPU";
	if (**it == 'M') dep_prior = "MEM";
	return;
}

/***********************************************
  ReadIn Part
***********************************************/
void getinfo(char ** info, map<string, vector<int>> &vtseries, string &dbeg, string &dend){
	string pstr("flavor");
	vector<int> vec;
	int i = 0;
	// get flavor info 
	while(*info[i] != 'M' && *info[i] != 'C'){
		string name("");
		for(int j = 0; *(info[i]+j) != ' ' && *(info[i]+j) != '\n'; j++){
			name.push_back(*(info[i]+j));
		}
		if(name.compare(0, 6, pstr) == 0){
			vtseries[name] = vec;
		}
		i++;
	}
	// get predict date info 
	char *t = info[i + 2];
	while (*t != ' ') dbeg.push_back(*(t++));
	t = info[i + 3];
	while (*t != ' ') dend.push_back(*(t++));
	return;
}
int to_day(int y, int m, int d) {
	int mon[] = { 0,31,28,31,30,31,30,31,31,30,31,30,31 };
	int day = 0;
	int i;
	for (i = 1; i<y; i++) {
		day += ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0) ? 366 : 365;
	}
	if ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0) mon[2]++;
	for (i = 1; i<m; i++) {
		day += mon[i];
	}
	return day + d;
}
int daycnt(string strbegin, string strend) {
	int yearbeg = stoi(strbegin.substr(0, 4));
	int yearend = stoi(strend.substr(0, 4));
	int monthbeg = stoi(strbegin.substr(5, 2));
	int monthend = stoi(strend.substr(5, 2));
	int daybeg = stoi(strbegin.substr(8, 2));
	int dayend = stoi(strend.substr(8, 2));
	return to_day(yearend, monthend, dayend) - to_day(yearbeg, monthbeg, daybeg);
}
void getdata(char ** data, int dataline, map<string, vector<int>> &vtseries, string &dend){
	vector<int> vec;
	// step_0 get begin date
	string date_beg("");
	int offset = 0;
	while (*(*data + offset++) != '\t');
	while (*(*data + offset++) != '\t');
	while (*(*data + offset) != ' ') {
		date_beg.push_back(*(*data + offset));
		offset++;
	}
	// step - liter all line
	while(dataline-- > 0){
		// step_1 get flavorxx
		string curflavor("");
		int i = 0;
		while (*(*data + i++) != '\t') ;
		while (*(*data + i) != '\t') {
			curflavor.push_back(*(*data + i));
			i++;
		}
		// step_2 find and save dateinfo
		if (vtseries.find(curflavor) != vtseries.end()) {
			string date_end("");
			i++;
			while (*(*data + i) != ' ') {
				date_end.push_back(*(*data + i));
				i++;
			}
			int day_offset = daycnt(date_beg, date_end);
			int cnt = day_offset + 1 - vtseries[curflavor].size();
			if (cnt == 0) vtseries[curflavor][day_offset]++;
			else {
				while (cnt-- > 0) vtseries[curflavor].push_back(0);
				vtseries[curflavor][day_offset]++;
			}
		}
		data += 1;
	}
	// step_3 add tail(0) to every vector<int>
	int sz = daycnt(date_beg, dend);
	for (auto it = vtseries.begin(); it != vtseries.end(); it++) {
		int addNum = sz - it->second.size();
		while (addNum-- > 0) it->second.push_back(0);
	}
    // denoise
    for (auto it = vtseries.begin(); it != vtseries.end(); it++) {
		size_t sz = vec.size();
		double E = 0, D = 0, std_D = 0;
	
		for (size_t i = 0; i < sz; i++) { E += vec[i]; }
		E = E / sz;
		for (size_t i = 0; i < sz; i++) { D += pow(vec[i] - E, 2); }
		D = D / sz;
		std_D = sqrt(D);

		double N = 2;
		for (size_t i = 0; i < sz; i++) {
			if (std_D > 3 && vec[i] > E + N * std_D) vec[i] = (int)(E + N * std_D);
			if (std_D > 3 && vec[i] < E - N * std_D){
				if (E - N * std_D > 0) vec[i] = (int)(E - N * std_D);
				else vec[i] = 0;
			}
		}

	}
}

/***********************************************
Predict Part
***********************************************/
struct Data
{
	vector<int> x;
	int y;
};

int pdt(vector<int> &pdt_x, const int n_pdt, const int offset) {
	double sum = 0;
	for (size_t i = 0; i < pdt_x.size(); i++) sum += pdt_x[i];
	sum = sum*n_pdt / pdt_x.size() + offset;
	if (sum < 0) sum = 0;
	return (int)sum;
}

double calLoss(const vector<int> &v, const int n_his, const int offset, const int n_pdt) {
	// get x y
	vector<Data> data_set;
	int n_datas = v.size() - n_his - n_pdt + 1;
	for (int i = 0; i < n_datas; i++) {
		Data data;
		vector<int> tp_feature(v.begin() + i, v.begin() + i + n_his);
		int tp_pdtval = 0;
		for (int j = i + n_his; j < i + n_his + n_pdt; j++) {
			tp_pdtval += v[j];
		}
		data.x = tp_feature;
		data.y = tp_pdtval;
		data_set.push_back(data);
	}
	// get y_predict and cal loss
	double loss = 0;
	for (size_t i = 0; i < data_set.size(); i++) {
		int y_pdt = pdt(data_set[i].x, n_pdt, offset);
		loss += pow(data_set[i].y - y_pdt, 2);
	}
	loss = loss/data_set.size();
	return loss;
}

void optimalFind(const vector<int> &his, const vector<int> &day_set, const vector<int> &offset_set, const int day_pdt, int &day_ret, int &offset_ret) {
	day_ret = 0, offset_ret = 0;
	double minLoss = DBL_MAX;
	for (size_t i = 0; i < day_set.size(); i++) {
		for (size_t j = 0; j < offset_set.size(); j++) {
			double loss = calLoss(his, day_set[i], offset_set[j], day_pdt);
			if (loss < minLoss) {
				day_ret = day_set[i];
				offset_ret = offset_set[j];
				minLoss = loss;
			}
		}
	}
}

void predict(map<string, vector<int>> &vtser, map<string, int> &vpdt, int tlen) {
	for (auto it = vtser.begin(); it != vtser.end(); it++) {
		vector<int> his = it->second;
		vector<int> day_set = { 5,6,7,8,9,10 }, offset_set = { -1,0,1 };
		int day_ret, offset_ret;
		int day_pdt = tlen;

		optimalFind(his, day_set, offset_set, day_pdt, day_ret, offset_ret);
        cout << day_ret << " " << offset_ret << endl;

		vector<int> x_pdt(his.end() - day_ret, his.end());
		int pdt_y = pdt(x_pdt, day_pdt, offset_ret);
		vpdt[it->first] = pdt_y;
	}
	return;
}

/***********************************************
Deploy Part
***********************************************/
// get flavor_predict bad to good sorted vector
class myCmp {
public:
	myCmp(const map<string, vector<int>> &flaInfo, const string &dep_prior) {
		myFlaInfo = flaInfo;
		if (dep_prior.compare("CPU") == 0) iscpu = true;
		else iscpu = false;
	}
	bool operator() (const pair<string, int>& lhs, const pair<string, int>& rhs) const {
		if (iscpu) {
			float i_lhs, i_rhs;
			auto it_lhs = myFlaInfo.find(lhs.first);
			auto it_rhs = myFlaInfo.find(rhs.first);
			i_lhs = float(it_lhs->second[0]) / it_lhs->second[1];
			i_rhs = float(it_rhs->second[0]) / it_rhs->second[1];
			if (i_lhs < i_rhs) return true;
			if (i_lhs > i_rhs) return false;
			if (it_lhs->second[0] < it_rhs->second[0]) return true;
			return false;
		}
		else {
			float i_lhs, i_rhs;
			auto it_lhs = myFlaInfo.find(lhs.first);
			auto it_rhs = myFlaInfo.find(rhs.first);
			i_lhs = float(it_lhs->second[1]) / it_lhs->second[0];
			i_rhs = float(it_rhs->second[1]) / it_rhs->second[0];
			if (i_lhs < i_rhs) return true;
			if (i_lhs > i_rhs) return false;
			if (it_lhs->second[1] < it_rhs->second[1]) return true;
			return false;
		}
	}
private:
	map<string, vector<int>> myFlaInfo;
	bool iscpu = true;
};
vector<pair<string, int>> getSortedVec(const map<string, vector<int>> &flaInfo, const map<string, int> &vpred, const string &dep_prior) {
	vector<pair<string, int>> ret;
	for (auto e : vpred) {
		if(e.second > 0) ret.push_back(e);
	}
	sort(ret.begin(), ret.end(), myCmp(flaInfo, dep_prior));
	return ret;
}

bool add_dep_bad(map<string, int> &phyId, vector<pair<string,int>> &sortedVpred,
	int &phy_cpu_used, int &phy_mem_used, const vector<int> phyInfo, const map<string, vector<int>> flaInfo) {
	if (sortedVpred.size() == 0) return false;
	int i = 0;
	string flavor_name = sortedVpred[i].first;
	phy_cpu_used += (flaInfo.find(flavor_name)->second)[0];
	phy_mem_used += (flaInfo.find(flavor_name)->second)[1];
	if (phy_cpu_used > phyInfo[0] || phy_mem_used > phyInfo[1]) {
		phy_cpu_used -= (flaInfo.find(flavor_name)->second)[0];
		phy_mem_used -= (flaInfo.find(flavor_name)->second)[1];
		for (; i < (int)sortedVpred.size(); i++) {
			flavor_name = sortedVpred[i].first;
			phy_cpu_used += (flaInfo.find(flavor_name)->second)[0];
			phy_mem_used += (flaInfo.find(flavor_name)->second)[1];
			if (phy_cpu_used > phyInfo[0] || phy_mem_used > phyInfo[1]) {
				phy_cpu_used -= (flaInfo.find(flavor_name)->second)[0];
				phy_mem_used -= (flaInfo.find(flavor_name)->second)[1];
				continue;
			}
			else break;
		}
		if(i == (int)sortedVpred.size()) return false;
	}
	// add to phyId
	if (phyId.find(flavor_name) == phyId.end()) {
		phyId[flavor_name] = 1;
	}
	else phyId[flavor_name]++;
	// erase from sortedVpred
	if(sortedVpred[i].second == 1) sortedVpred.erase(sortedVpred.begin());
	else sortedVpred[i].second -= 1;
	return true;
}
bool add_dep_good(map<string, int> &phyId, vector<pair<string, int>> &sortedVpred,
	int &phy_cpu_used, int &phy_mem_used, const vector<int> phyInfo, const map<string, vector<int>> flaInfo) {
	if (sortedVpred.size() == 0) return false;
	int i = sortedVpred.size() - 1;
	string flavor_name = sortedVpred[i].first;
	phy_cpu_used += (flaInfo.find(flavor_name)->second)[0];
	phy_mem_used += (flaInfo.find(flavor_name)->second)[1];
	if (phy_cpu_used > phyInfo[0] || phy_mem_used > phyInfo[1]) {
		phy_cpu_used -= (flaInfo.find(flavor_name)->second)[0];
		phy_mem_used -= (flaInfo.find(flavor_name)->second)[1];
		for (; i >= 0; i--) {
			flavor_name = sortedVpred[i].first;
			phy_cpu_used += (flaInfo.find(flavor_name)->second)[0];
			phy_mem_used += (flaInfo.find(flavor_name)->second)[1];
			if (phy_cpu_used > phyInfo[0] || phy_mem_used > phyInfo[1]) {
				phy_cpu_used -= (flaInfo.find(flavor_name)->second)[0];
				phy_mem_used -= (flaInfo.find(flavor_name)->second)[1];
				continue;
			}
			else break;
		}
		if (i == -1) return false;
	}
	if (phyId.find(flavor_name) == phyId.end()) {
		phyId[flavor_name] = 1;
	}
	else phyId[flavor_name]++;
	if(sortedVpred[i].second == 1) sortedVpred.erase(sortedVpred.end() - 1);
	else sortedVpred[i].second -= 1;
	return true;
}

void deploy(const vector<int> &phyInfo, const map<string, vector<int>> &flaInfo, const map<string, int> &vpred, map<int, map<string, int>> &dep, string &dep_prior) {
	vector<pair<string, int>> sortedVpred = getSortedVec(flaInfo, vpred, dep_prior);
	int phyId = 0;
	int phy_cpu_used = 0;
	int phy_mem_used = 0;
	float used_rate = 0;
	bool canadd = true;
	map<string, int> init;
	dep[0] = init;
	// cpu prior
	if(dep_prior.compare("CPU") == 0){
		float stand_rate = float(phyInfo[0]) / phyInfo[1];
		while (sortedVpred.size() > 0) {
			// full one physical machine
			while (canadd && sortedVpred.size() > 0) {
				if (phy_cpu_used == 0 && phy_mem_used == 0) {
					canadd = add_dep_bad(dep[phyId], sortedVpred, phy_cpu_used, phy_mem_used, phyInfo, flaInfo);
					used_rate = float(phy_cpu_used) / phy_mem_used;
				}
				// bad resource has more , should add good resource
				if (used_rate < stand_rate) {
					canadd = add_dep_good(dep[phyId], sortedVpred, phy_cpu_used, phy_mem_used, phyInfo, flaInfo);
				}
				else {
					canadd = add_dep_bad(dep[phyId], sortedVpred, phy_cpu_used, phy_mem_used, phyInfo, flaInfo);
				}
				used_rate = float(phy_cpu_used) / phy_mem_used;
			}
			// phyid->phy machine is full , should get new one;
			dep[++phyId] = init;
			phy_cpu_used = 0;
			phy_mem_used = 0;
			used_rate = 0;
			canadd = true;
		}
	}
	// mem prior
	else {
		float stand_rate = float(phyInfo[1]) / phyInfo[0];
		while (sortedVpred.size() > 0) {
			// full one physical machine
			while (canadd && sortedVpred.size() > 0) {
				if (phy_cpu_used == 0 && phy_mem_used == 0) {
					canadd = add_dep_bad(dep[phyId], sortedVpred, phy_cpu_used, phy_mem_used, phyInfo, flaInfo);
					used_rate = float(phy_mem_used) / phy_cpu_used;
				}
				// bad resource has more , should add good resource
				if (used_rate < stand_rate) {
					canadd = add_dep_good(dep[phyId], sortedVpred, phy_cpu_used, phy_mem_used, phyInfo, flaInfo);
				}
				else {
					canadd = add_dep_bad(dep[phyId], sortedVpred, phy_cpu_used, phy_mem_used, phyInfo, flaInfo);
				}
				used_rate = float(phy_mem_used) / phy_cpu_used;
			}
			// phyid->phy machine is full , should get new one;
			dep[++phyId] = init;
			phy_cpu_used = 0;
			phy_mem_used = 0;
			used_rate = 0;
			canadd = true;
		}
	}

	auto it = dep.find(phyId);
	if (it->second.size() == 0) dep.erase(it);
	return;
}

/***********************************************
Program Data Form -> File Save Needed Form 
***********************************************/
char* getResFile(map<string, int> &vpred, map<int, map<string, int>> &dep) {
	char * res;
	string buff;
	int flavorSum = 0;
	for (auto it = vpred.begin(); it != vpred.end(); it++) flavorSum += it->second;
	buff += to_string(flavorSum) + "\n";
	for (auto it = vpred.begin(); it != vpred.end(); it++) buff += it->first + " " + to_string(it->second) + "\n";
	buff += "\n" + to_string(dep.size()) + "\n";
	for (auto it = dep.begin(); it != dep.end(); it++) {
		buff += to_string(it->first);
		map<string, int> phyId_FlaDep = it->second;
		for (auto it2 = phyId_FlaDep.begin(); it2 != phyId_FlaDep.end(); it2++) {
			buff += " " + it2->first + " " + to_string(it2->second);
		}
		buff += "\n";
	}
	// string to char*
	res = new char[buff.size()];
	char *cur = res;
	for (auto it = buff.begin(); it != buff.end() - 1;) *cur++ = *it++;
	*cur = '\0';
	return res;
}
