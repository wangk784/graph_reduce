#include <iomanip>
#include "gr.h"


#define reduce(i) cs[i].state = State::Delete;

long current_sol;

void buger()
{
    printf(" *\n");
}

void init() {
    std::cout << "initializing.." << endl;
    best_value = LONG_MAX; //取最大值
    cs_size = 0;
    current_sol = 0;
    t_length = 0;
    uncover_num = 0;
    //memset(t_index, -1, sizeof(t_index));
    for (size_t i = 0; i < vertex_num; i++)
    {
        t_index[i] = -1;
        uncover_vertex_index[i] = i;
        uncover_vertex[i] = i;
        uncover_num++;
    }
}

//用cplex求解初始图
void cplex_result_origin_graph()
{
    IloEnv env;
    try {
        IloModel model(env);
        IloNumVarArray vars(env);
        for (size_t i = 0; i < vertex_num; i++)
        {
            if (cs[i].state == State::Candidate)
            {
                //添加变量，取值为{0,1}
                vars.add(IloNumVar(env, 0, 1, IloNumVar::Int, ("x"+to_string(i)).c_str()));
            }
            else if(cs[i].state == State::Forbid || cs[i].state == State::Delete)
            {
                vars.add(IloNumVar(env, 0, 0, IloNumVar::Int, ("x"+to_string(i)).c_str()));
            }
            else
            {
                vars.add(IloNumVar(env, 1, 1, IloNumVar::Int, ("x"+to_string(i)).c_str()));
            }
        }
        //构造目标函数
        IloExpr obj = vars[0];
        for (size_t i = 1; i < vertex_num; i++)
        {
            obj += vars[i];
        }
        model.add(IloMinimize(env, obj));
        
        for (size_t i = 0; i < vertex_num; i++)
        {
            IloExpr constraint = vars[i];
            for (size_t j = 0; j < vertex_neightbourNum[i]; j++)
            {
                int v_n = vertex[i][j];
                constraint += vars[v_n];
            }
            model.add(constraint >= 1);
        }
        IloCplex cplex(model);
        if (!cplex.solve())
        {
            env.error() << "Failed to optimize LP." << endl;
            throw(-1);
        }
        else
        {
            cout << "Success!" << endl;
            IloNumArray vals(env);
            env.out() << "Solution status = " << cplex.getStatus() << endl;
            cplex.getValues(vals, vars);
            int lock_num = 0;
            for (size_t i = 0; i < vertex_num; i++)
            {
                if (vals[i] == 1)
                {
                    lock_num++;
                }
            }
            cout << "lock_num:" << lock_num << endl;
        }
    }
    catch (IloException & e) {
        cerr << "Concert exception caught: " << e << endl;
    }
    catch (...) {
        cerr << "Unknown exception caught" << endl;
    }
    env.end();
}


//用cplex求解剩余图
void cplex_result_reduce_graph()
{
    IloEnv env;
    try {
        IloModel model(env);
        IloNumVarArray vars(env);
        vector<int> remain_vertex_index(vertex_num);
        vector<int> candidate_vertex(vertex_num);
        int remain_cnt = 0;     //剩余顶点数量
        for (size_t i = 0; i < vertex_num; i++)
        {
            if (cs[i].state == State::Candidate)
            {
                //添加变量，取值为{0,1}
                vars.add(IloNumVar(env, 0, 1, IloNumVar::Int, ("x"+to_string(i)).c_str()));
                // vars.add(IloNumVar(env, 0.0, 1.0));
                remain_vertex_index[i] = remain_cnt;
                candidate_vertex[remain_cnt] = i;
                remain_cnt++;
            }
        }
        //构造目标函数
        IloExpr obj = vars[0];
        for (size_t i = 1; i < remain_cnt; i++)
        {
            obj += vars[i];
        }
        model.add(IloMinimize(env, obj));
        
        int constraint_cnt = 0;
        for (size_t i = 0; i < vertex_num; i++)
        {
            //把所有未支配顶点的邻居中剩余顶点作为约束条件
            if (cs[i].num_in_c > 0)
            {
                continue;
            }
            constraint_cnt++;
            //IloExpr声明时必须初始化，设置一个为0的IloIntVar作为初始表达式
            IloIntVar initExpr(env, 0, 0);
            //声明约束表达式
            IloExpr constraint = initExpr;
            //如果未支配顶点是剩余顶点，则把自身也添加到约束条件
            if (cs[i].state == State::Candidate)
            {
                constraint += vars[remain_vertex_index[i]];
            }
            //把未支配的顶点邻居里的剩余顶点加入约束条件
            for (size_t j = 0; j < vertex_neightbourNum[i]; j++)
            {
                int v_n = vertex[i][j];
                if (cs[v_n].state == State::Candidate)
                {
                    constraint += vars[remain_vertex_index[v_n]];
                }
            }
            //把约束条件添加到模型里
            model.add(constraint >= 1);
        }
        // cout << "constraint_cnt:" << constraint_cnt << endl;
        IloCplex cplex(model);
        //cplex求解
        if (!cplex.solve())
        {
            env.error() << "Failed to optimize LP." << endl;
            throw(-1);
        }
        else
        {
            IloNumArray vals(env);
            env.out() << "Solution status = " << cplex.getStatus() << endl;
            cplex.getValues(vals, vars);
            int lock_num = 0;
            double obj_value = 0.0;
            for (size_t i = 0; i < remain_cnt; i++)
            {
                if (vals[i] != 0)
                {
                    lock_num++;
                }
                obj_value += vals[i];
            }
            cout << "objective expr: " << obj_value << endl;
            cout << "lock_num:" << lock_num << endl;
            cout << "total_lock:" << lock_num + total_lock << endl;
            add_select_vertex(candidate_vertex, vals, remain_cnt);
        }
    }
    catch (IloException & e) {
        cerr << "Concert exception caught: " << e << endl;
    }
    catch (...) {
        cerr << "Unknown exception caught" << endl;
    }
    env.end();
}

//添加cplex选出来的顶点
void add_select_vertex(vector<int> vec_remain, IloNumArray vals, int remain_cnt)
{
    for (size_t i = 0; i < remain_cnt; i++)
    {
        if (vals[i] == 1)
        {
            lock_vertex(vec_remain[i]);
        }
    }
}

void init_reduce()
{
    int i, j;
    int a, b, c, sum;
    int v_neighbor, neighbor_num;
    times(&start);
    std::cout << "first step--->: init reduce" << endl;
    for (i = 0; i < vertex_num; i++)
    {
        if (vertex_neightbourNum[i] == 1)
        {
            v_neighbor = vertex[i][0];
            if (cs[i].state != State::Fixed)
            {
                reduce(i);
                lock_vertex(v_neighbor);
            }
        }
        else if (vertex_neightbourNum[i] == 2)
        {
            if (vertex_neightbourNum[vertex[i][0]] == 2)
            {
                a = vertex[i][0];
                b = vertex[i][1];
            }
            else if (vertex_neightbourNum[vertex[i][1]] == 2)
            {
                a = vertex[i][1];
                b = vertex[i][0];
            }
            else
                continue;
            if (vertex[a][0] == i)
                c = vertex[a][1];
            else
                c = vertex[a][0];
            if (b == c)
            {
                if (cs[i].state != State::Fixed && cs[a].state != State::Fixed)
                {
                    reduce(i);
                    reduce(a);
                    lock_vertex(b);
                }
            }
        }
        else if (vertex_neightbourNum[i] == 0)
        {
            lock_vertex(i);
        }
        else
        {
            neighbor_num = vertex_neightbourNum[i];
            sum = 0;
            for (j = 0; j < neighbor_num; j++)
                if (vertex_neightbourNum[vertex[i][j]] == 1)
                    sum += cs[vertex[i][j]].cost;
            if (sum > cs[i].cost)
            {
                for (j = 0; j < neighbor_num; j++)
                {
                    if (vertex_neightbourNum[vertex[i][j]] == 1)
                    {
                        reduce(vertex[i][j]);
                    }
                }
                lock_vertex(i);
            }
        }
    }
    int kkk, k;
    int lenn;
    remain_num = 0;
    for (i = 0; i < vertex_num; i++)
    {
        if (cs[i].state == State::Delete)
        {
            //从图中删除该节点
            neighbor_num = vertex_neightbourNum[i];
            for (j = 0; j < neighbor_num; j++)
            {
                v_neighbor = vertex[i][j];
                lenn = vertex_neightbourNum[v_neighbor];
                for (k = 0; k < lenn; k++)
                    if (vertex[v_neighbor][k] == i)
                        break;
                vertex_neightbourNum[v_neighbor]--;
                kkk = vertex[v_neighbor][vertex_neightbourNum[v_neighbor]];
                vertex[v_neighbor][vertex_neightbourNum[v_neighbor]] = vertex[v_neighbor][k];
                vertex[v_neighbor][k] = kkk;
            }
        }
        else
        {
            //uncover_vertex[remain_num] = i;
            //uncover_vertex_index[i] = remain_num;
            remain_vertex[remain_num++] = i;
        }
    }
}

//顶点从未支配中移除
void remove_from_uncover(int v)
{
    uncover_num--;
    // int uk = uncover_vertex_index[v];
    // int ck = uncover_vertex[uncover_num];
    // uncover_vertex[uk] = ck;
    // uncover_vertex_index[ck] = uk;
}

//顶点score值改变，判断加入t还是移出t
void modify_t(int v)
{
    if (cs[v].score == 0)
        reduce(v);
    // if (cs[v].score == 0 && t_index[v] == -1 && cs[v].state != State::Fixed)
    // {
    //     t[t_length] = v;
    //     t_index[v] = t_length++;
    // }
    // else if (t_index[v] != -1 && cs[v].score != 0)
    // {
    //     t_length--;
    //     int uk = t_index[v];
    //     int ck = t[t_length];
    //     t[uk] = ck;
    //     t_index[ck] = uk;
    //     t_index[v] = -1;
    // }
}

//把顶点c固定
void lock_vertex(int c)
{
    if (cs[c].state == State::Fixed)
        return;
    total_lock++;
    cs[c].state = State::Fixed;
    cs[c].score = -cs[c].score;
    if (cs[c].score == 0 && t_index[c] == -1 && cs[c].state != State::Fixed)
    {
        t[t_length] = c;
        t_index[c] = t_length++;
    }
    if (cs[c].num_in_c == 0)
    	remove_from_uncover(c);
    for (size_t h = 0; h < vertex_neightbourNum[c]; h++)
    {
        int v_n = vertex[c][h];
        if (cs[v_n].num_in_c == 0)
        	remove_from_uncover(v_n);
        cs[v_n].num_in_c++;
        if (cs[c].num_in_c == 0)
        {
            //v_n被支配，修改score
            //c 支配次数 0->1
            cs[v_n].score -= vertex_weight[c];
            modify_t(v_n);
        }
        else if (cs[c].num_in_c == 1 && cs[v_n].state == State::Fixed)
        {
            //v_n存在在候选解, 此时v_n的score值为负
            //c 支配次数 1->2
            cs[v_n].score += vertex_weight[c];
            modify_t(v_n);
        }
        if (cs[v_n].state == State::Fixed)
        {
            if (cs[v_n].num_in_c == 2)
                //v_n已经在候选解，c加入候选解后，v_n又被支配了一次
                cs[v_n].score += vertex_weight[v_n];
            modify_t(v_n);
            continue;
        }
        //处理二层邻居
        int cnt = 0;
        int s = 0;
        for (size_t l = 0; l < vertex_neightbourNum[v_n]; l++)
        {
            int j = vertex[v_n][l];
            if (j == c)
                continue;
            if (cs[j].state == State::Fixed)
            {
                s = j;
                cnt++;
            }
        }
        if (cs[v_n].state == State::Fixed)
        {
            s = v_n;
            cnt++;
        }
        if (cnt == 0)
        {
            //c是i的邻居里面第一个加入候选解的
            cs[v_n].score -= vertex_weight[v_n];
            modify_t(v_n);
            for (size_t l = 0; l < vertex_neightbourNum[v_n]; l++)
            {
                int j = vertex[v_n][l];
                if (j == c)
                    continue;
                //i被c支配，所以二层邻居j score值减少
                cs[j].score -= vertex_weight[v_n];
                modify_t(j);
            }
        }
        else if (cnt == 1)
        {
            //c是这里组里第二个加入候选解的顶点
            cs[s].score += vertex_weight[v_n];
            modify_t(s);
        }
        modify_t(v_n);
    }
    cs[c].num_in_c++;
}

void superset_reduce()
{
    std::cout << "second step--->: superset reduce" << endl;
    queue<int> q_searchset;
    for (size_t i = 0; i < vertex_num; i++)
        if (cs[i].num_in_c == 0)
        {
            cs[i].is_in_search = 1;
            q_searchset.push(i);
        }
    while (!q_searchset.empty()) {
        //时间计算
        times(&finish);
        double tt = double(finish.tms_utime - start.tms_utime + finish.tms_stime - start.tms_stime) / sysconf(_SC_CLK_TCK);
        tt = round(tt * 100) / 100.0;
        if (time_limit != 0 && tt > time_limit)
            break;
        int v = q_searchset.front();
        q_searchset.pop();
        cs[v].is_in_search = 0;
        if (cs[v].num_in_c != 0)
            continue;
        int set_count = 0;
        //存放v闭邻居score>0集合
        vector<int> node;
        vector<int> node_score;
        node.push_back(v);
        node_score.push_back(cs[v].score);
        set_count++;
        for (int j = 0; j < vertex_neightbourNum[v]; ++j) {
            int v_neighbor = vertex[v][j];
            if (cs[v_neighbor].score > 0) {
                node.push_back(v_neighbor);
                node_score.push_back(cs[v_neighbor].score);
                ++set_count;
            }
        }
        if (set_count == 1)
            lock_vertex(v);
        else {
            //寻找score最大的节点
            int max_score_index = 0;
            int max_score = node_score[0];
            for (int i = 1; i < set_count; ++i) {
                if (node_score[i] > max_score) {
                    max_score = node_score[i];
                    max_score_index = i;
                }
            }
            int max_score_v = node[max_score_index];
            auto max_set = new NeighborSet(max_score_v);
            int cnt = 0;
            //初始化超集候选集合元素
            if (cs[max_score_v].num_in_c == 0)
            {
                max_set->addNeighbor(max_score_v);
                cnt++;
            }
            for (int i = 0; i < vertex_neightbourNum[max_score_v] && cnt < max_score; ++i) {
                int v_neighbor = vertex[max_score_v][i];
                if (cs[v_neighbor].num_in_c == 0)
                {
                    max_set->addNeighbor(v_neighbor);
                    cnt++;
                }
            }
            //判断score值最大的集合是不是其他集合的超集
            bool is_super_set = true;
            for (int i = 0; i < set_count; ++i) {
                if (node[i] == max_score_v)
                    continue;
                int v_neighbor = node[i];
                int v_neighbor_score = node_score[i];
                cnt = 0;
                if (cs[v_neighbor].num_in_c == 0)
                {
                    if (!max_set->is_in_set(v_neighbor))
                    {
                        is_super_set = false;
                        break;
                    }
                    else
                        cnt++;
                }
                for (int j = 0; j < vertex_neightbourNum[v_neighbor] && cnt < v_neighbor_score; ++j) {
                    int v_neighbor_neighbor = vertex[v_neighbor][j];
                    if (cs[v_neighbor_neighbor].num_in_c == 0) {
                        if (max_set->is_in_set(v_neighbor_neighbor)) {
                            cnt++;
                        } else {
                            is_super_set = false; //不是超集，跳出循环
                            break;
                        }
                    }
                }
                if (!is_super_set)
                    break;
            }
            if (is_super_set)
            {
                lock_vertex(max_set->v);
                //max_set->v的所有未覆盖的二层邻居都加入队列
                for (size_t i = 0; i < vertex_neightbourNum[max_set->v]; i++) {
                    int v_n = vertex[max_set->v][i];
                    for (size_t j = 0; j < vertex_neightbourNum[v_n] ; j++) {
                        int v_n_n = vertex[v_n][j];
                        //v_n_n邻居有未被支配的顶点
                        //v_n的支配次数不止一次，也就是在加入顶点max_set->v后，v_n_n的score值发生变化
                        if (cs[v_n_n].score > 0 && cs[v_n].num_in_c == 1) 
                        {
                            int cnt2 = 0;
                            if (cs[v_n_n].num_in_c == 0 && cs[v_n_n].is_in_search == 0)
                            {
                                cs[v_n_n].is_in_search = 1;
                                q_searchset.push(v_n_n);
                                cnt2++;
                            }
                            for (size_t k = 0; k < vertex_neightbourNum[v_n_n] && cnt2 < cs[v_n_n].score; k++)
                            {
                                int v_n_n_n = vertex[v_n_n][k];
                                //未支配且不在搜索集的三层邻居加入搜索集
                                if (cs[v_n_n_n].num_in_c == 0 && cs[v_n_n_n].is_in_search == 0)
                                {
                                    cs[v_n_n_n].is_in_search = 1;
                                    q_searchset.push(v_n_n_n);
                                    cnt2++;
                                }
                            }
                        }
                    }
                }
            }
            delete max_set;
        }
    }
}

void subset_reduce()
{
    cout << "third step--->:subset reduce" <<endl;
    for (size_t i = 0; i < vertex_num; i++)
    {
        if (cs[i].state == State::Candidate)
        {
            int cnt = 1;    //i本身也要存在集合里面, 记录i的闭邻居集合有多少元素
            for (size_t j = 0; j < vertex_neightbourNum[i]; j++)
            {
                int v_n = vertex[i][j];
                // && cs[v_n].is_exclude != 1
                if (cs[v_n].score > 0 )
                {
                    cnt++;
                }
            }
            //构造所有邻居集合
            vector<NeighborSet*> sets(cnt);
            sets[0] = new NeighborSet(i);
            sets[0]->addNeighbor(i);
            int cnt1 = 0;
            for (size_t j = 0; j < vertex_neightbourNum[i] && cnt1 < cnt; j++)
            {
                int v_n = vertex[i][j];
                if (cs[v_n].score > 0)
                {
                    if (cs[v_n].num_in_c == 0)
                        sets[0]->addNeighbor(v_n);
                    cnt1++;
                    sets[cnt1] = new NeighborSet(v_n);
                    int cnt2 = 0;
                    if (cs[v_n].num_in_c == 0)
                    {
                        sets[cnt1]->addNeighbor(v_n);
                        cnt2++;
                    }
                    for (size_t k = 0; k < vertex_neightbourNum[v_n] && cnt2 < cs[v_n].score; k++)
                    {
                        int v_n_n = vertex[v_n][k];
                        if (cs[v_n_n].num_in_c == 0)
                        {
                            sets[cnt1]->addNeighbor(v_n_n);
                            cnt2++;
                        }
                    }
                }
            }
            //循环遍历删除子集顶点
            for (size_t j = 0; j < sets.size(); j++)
            {
                for (size_t k = j + 1; k < sets.size(); k++)
                {
                    auto set_a = sets[j];
                    auto set_b = sets[k];
                    //外层节点已经被删除，打断内部for循环
                    if (cs[set_a->v].state == State::Forbid)
                    {
                        break;
                    }
                    //内部节点已经被删除，跳过本次内部循环
                    if (cs[set_b->v].state == State::Forbid)
                    {
                        continue;
                    }
                    /*
                        如果set_b元素数量比set_a多，交换指针，set_b始终是待判断子集的指针
                        如果两个顶点的有效邻居元素数量相等，则判断是否为互为子集
                        如果互为子集，则选择一个排除
                    */
                    if (set_a->getValidNeighborCnt() < set_b->getValidNeighborCnt())
                    {
                        auto temp = set_b;
                        set_b = set_a;
                        set_a = temp;
                    }
                    //判断set_b是不是set_a的子集
                    bool is_subset = true;
                    for (size_t s = 0; s < set_b->getNeighborCnt(); s++)
                    {
                        //已支配顶点不用判断
                        if (cs[set_b->neighbors[s]].num_in_c == 0 &&  !set_a->is_in_set(set_b->neighbors[s]))
                        {
                            is_subset = false;
                            break;
                        }
                    }
                    if (is_subset)
                    {
                        //set_b是set_a的子集，把set_b的头元素即v排除在候选解之外
                        cs[set_b->v].state = State::Forbid;
                    }
                }
            }
            //free memory
            for (auto & set : sets)
            {
                delete set;
            }
        }
    }
}

void print_reduce_graph_info()
{
    int locked_num = 0;         //确定在最优解的顶点数量
    int uncover_num = 0;        //经过两次reduce后未支配顶点数量
    int remove_num = 0;         //确定删除的顶点数量
    remain_num = 0;                 //经过reduce后剩余顶点数量
    int forbid_add_num = 0;     //subset_reduce确定一定不再最优解的顶点数量
    int dominate_num = 0;
    for (size_t i = 0; i < vertex_num; i++)
    {
        switch (cs[i].state)
        {
        case State::Candidate:
            remain_num++;
            break;
        case State::Fixed:
            locked_num++;
            break;
        case State::Delete:
            remove_num++;
            break;
        case State::Forbid:
            forbid_add_num++;
            break;
        default:
            break;
        }
        if (cs[i].num_in_c == 0)
        {
            uncover_num++;
        }
        else
        {
            dominate_num++;
        }
    }
    std::cout << "Total Vertex: " << vertex_num << endl;
    std::cout << "Delete Vertex: " << remove_num << endl;
    std::cout << "Forbid Add: " << forbid_add_num << endl;
    std::cout << "Fixed Vertex: " << locked_num << endl;
    std::cout << "Remain Vertex:" << remain_num << endl;
    std::cout << "Cover Vertex: " << dominate_num << endl;
    std::cout << "Uncover Vertex: " << uncover_num << endl;
    // std::cout << "Percent: " <<  fixed << setprecision(2) << remain_num * 1.0 / vertex_num * 100 << "%" << endl;
}

void print_density(int idx)
{
    int edge_cnt = 0;
    int vertex_cnt = 0;
    for (size_t i = 0; i < vertex_num; i++)
    {
        if (cs[i].state == State::Candidate)
        {
            vector<int> neighbors;
            for (int j = 0; j < vertex_neightbourNum[i]; ++j) {
                int v_n = vertex[i][j];
                if (cs[v_n].state == State::Candidate || cs[v_n].state == State::Forbid)
                {
                    neighbors.push_back(v_n);
                    edge_cnt++;
                }
            }
            vertex_cnt++;
        }
    }
   cout << "Density1: " << fixed << setprecision(3) << edge_num *1.0 / vertex_num / (vertex_num - 1) * 100 << "%" << endl;
   if (vertex_cnt != 0)
   {
       cout << "Density2: " << fixed << setprecision(3) << edge_cnt * 1.0 / vertex_cnt / (vertex_cnt - 1) * 100 << "%" << endl;
   }
   else
   {
       cout << "Density2: 0" << endl;
   }
}

void generate_reduce_graph(int step)
{
    string new_file_name = filename + "_r" + to_string(step);
    std::ofstream openfile(new_file_name, std::ios::out);
    for (size_t i = 0; i < vertex_num; i++)
    {
        if (cs[i].state != State::Candidate)
            continue;
        vector<int> neighbors;
        for (int j = 0; j < vertex_neightbourNum[i]; ++j) {
            int v_n = vertex[i][j];
            if (cs[v_n].state == State::Fixed || cs[v_n].state == State::Delete || cs[v_n].num_in_c != 0)
                continue;
            neighbors.push_back(v_n);
        }
        if (neighbors.size() == 0)
            continue;
        if (cs[i].num_in_c == 0)
        {
            openfile << i + 1 << " " << i + 1;
        }
        else
        {
            openfile << i + 1;
        }
        for (size_t j = 0; j < neighbors.size(); j++)
        {
            openfile << " " << neighbors[j] + 1;
        }
        openfile << endl;
        if (step == 2)
            candidate.push_back(i);
    }
    openfile.close();
}

void print_degree()
{
    int d1 = 0, d2 = 0, d3 = 0;
    for (size_t i = 0; i < vertex_num; i++)
    {
        if (cs[i].state != State::Candidate) 
            continue;
        int n_cnt = 0;
        for (size_t j = 0; j < vertex_neightbourNum[i]; j++)
        {
            int v_n = vertex[i][j];
            if (cs[v_n].state == State::Candidate || cs[v_n].state == State::Forbid)
            {
                n_cnt++;
            }
        }
        if (n_cnt == 1)
        {
            d1++;
        }
        else if (n_cnt == 2)
            d2++;
        else if (n_cnt == 3)
            d3++;
    }
    cout << "d1:" << d1 << endl;
    cout << "d2:" << d2 << endl;
    cout << "d3:" << d3 << endl;
}

// check if the solution is a correct cover
int check(){ 
    cout << "checking.." << endl;
    int valid_size = candidate.size();
    int cnt = 0;
    while(!is_all_dominated() && valid_size != -1)
    {
        int max_score = 0;
        int max_index = 0;
        for (size_t i = 0; i < valid_size; i++)
        {
            if (cs[candidate[i]].score > max_score)
            {
                max_index = i;
                max_score = cs[candidate[i]].score;
            }
        }
        lock_vertex(candidate[max_index]);
        cnt++;
        candidate[max_index] = candidate[valid_size-- - 1];
    }
    if (is_all_dominated())
    {
        cout << "check lock num: " << cnt << endl;
        cout << "All coverd!" << endl;
    }
    else{
        cout << "Error: Candidate can't cover all vertex." << endl;
    }
    cnt = 0;
    for (size_t i = 0; i < vertex_num; i++)
    {
        if (cs[i].state == State::Fixed)
        {
            cnt++;
        }
    }
    cout << "Lock num: " << cnt << endl;
    return 1;
}

int main(int argc, char *argv[]){

    if(argc<2){
        printf("input wrong\n");
        return 0;
    }

    build_instance_massive(argv[1]);
    time_limit=atof(argv[2]);

    init();
    // cplex_result_origin_graph();
    //初始reduce
    init_reduce();
    print_reduce_graph_info();
    //超集缩减
    superset_reduce();
    print_reduce_graph_info();
    // generate_reduce_graph(1);
    //子集缩减
    subset_reduce();
    print_reduce_graph_info();
    generate_reduce_graph(2);
    // cplex_result_reduce_graph();
    
    //检查是否为正确解
    if (is_all_dominated())
    {
        cout << "Check OK" << endl;
    }
    
    //释放内存
    free_all();
    return 0;
}
