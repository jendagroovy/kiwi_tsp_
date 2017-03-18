// vim: let g:syntastic_cpp_compiler_options=' -std=c++0x '

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <stack>
#include <ctime>

#include "csv.h"

struct route_t;

struct route_ptr_compare_t {
    bool operator () (const route_t* const &lhs, const route_t* const &rhs) const;
};

struct node_t {
    node_t(std::string name, uint16_t idx) : name(name), idx(idx) {
        routes.resize(300);
        for (auto it = routes.begin(); it != routes.end(); ++it) {
            it->resize(300, NULL);
        }

    };

    void add_route(uint16_t day, uint16_t dest_idx, route_t * route) {
    /*
        if (routes.size() <= day) {
            routes.resize(day + 1);
        }

        if (routes[day].size() <= dest_idx) {
            routes[day].resize(dest_idx + 1, NULL);
        }
*/
        routes[day][dest_idx] = route;
    }

    uint16_t idx;
    std::string name;
    std::vector< std::vector<route_t*> > routes;
};

struct route_t {
    route_t(node_t *src, node_t *dest, uint16_t price):
        src(src), dest(dest), price(price),
        path_prev(NULL)
    {};

    node_t *src, *dest;
    uint16_t price;

    route_t *path_prev;
};

enum op_t {
    FORTH, BACK
};

struct stack_op_t {
    stack_op_t(op_t op, route_t * route) : op(op), route(route) {};
    op_t op;
    route_t *route;
};

struct semimatrix_t {
    semimatrix_t(uint16_t node_count, int threshold) : node_count(node_count), current_max(threshold), threshold(threshold) {
        storage = new int*[node_count];
        for (uint16_t i = 2; i < node_count; ++i) {
            storage[i] = new int[i - 1];
            for (uint16_t j = 0; j < i -1; j++) {
                storage[i][j] = 0;
            }
        }
    }
    ~semimatrix_t() {
        for (uint16_t i = 2; i < node_count; ++i) {
            delete [] storage[i];
        }
        delete [] storage;
    }

    void set(uint16_t node1, uint16_t node2) {
        current_max++; // TODO handle overflow
        storage[node1][node2] = current_max;
    }

    bool applies(uint16_t node1, uint16_t node2) {
        return storage[node1][node2] > current_max - threshold;
    }

    void inc(uint16_t node1, uint16_t node2) {
        storage[node1][node2]++;
    }

    int get(uint16_t node1, uint16_t node2) {
        return storage[node1][node2];
    }

    void get_minimum(uint16_t * node1, uint16_t * node2) {
        *node1 = 2;
        *node2 = 1;
        int min = -1;

        for (uint16_t i = 2; i < node_count; ++i) {
            for (uint16_t j = 1; j < i - 1; ++j) {
                if (min == -1 || storage[i][j] < min) {
                    min = storage[i][j];
                    *node1 = i;
                    *node2 = j;
                }
            }
        }

    }

    void clear() {
        for (uint16_t i = 2; i < node_count; ++i) {
            for (uint16_t j = 0; j < i - 1; ++j) {
                storage[i][j] = 0;
            }
        }
    }

    int **storage;
    uint16_t node_count;
    int current_max;
    int threshold;
}; 

struct neighbour_t {
    uint16_t i,j;
    int price;

    neighbour_t() : i(0), j(0), price(0) {};
    neighbour_t(uint16_t i, uint16_t j, int price) : i(i), j(j), price(price) {};

    void apply(std::vector<route_t*> &path) {
        node_t * node_i_pre = path[i-1]->src;
        node_t * node_i = path[i]->src;
        node_t * node_i_post = path[i]->dest;

        node_t * node_j_pre = path[j-1]->src;
        node_t * node_j = path[j]->src;
        node_t * node_j_post = path[j]->dest;


        path[i] = node_j->routes[i][node_i_post->idx];
        path[j-1] = node_j_pre->routes[j-1][node_i->idx];

        if (i - j > 1) {
            path[i-1] = node_i_pre->routes[i-1][node_j->idx];
            path[j] = node_i->routes[j][node_j_post->idx];
        } else {
            path[j] = node_i->routes[j][node_j->idx];
        }
    };

    bool try_apply(std::vector<route_t*> &path) {
        node_t * node_i_pre = path[i-1]->src;
        node_t * node_i = path[i]->src;
        node_t * node_i_post = path[i]->dest;

        node_t * node_j_pre = path[j-1]->src;
        node_t * node_j = path[j]->src;
        node_t * node_j_post = path[j]->dest;


        route_t * new_i_right = node_j->routes[i][node_i_post->idx];
        route_t * new_j_left = node_j_pre->routes[j-1][node_i->idx];

        if (new_i_right == NULL || new_j_left == NULL) return false;

        route_t * new_i_left;
        route_t * new_j_right;

        if (i - j > 1) {
            route_t * new_i_left = node_i_pre->routes[i-1][node_j->idx];
            new_j_right = node_i->routes[j][node_j_post->idx];

            if (new_i_left == NULL || new_j_right == NULL) return false;

            path[i-1] = new_i_left;
        } else {
            new_j_right = node_i->routes[j][node_j->idx];

            if (new_j_right == NULL) return false;
        }

        path[j] = new_j_right;
        path[i] = new_i_right;
        path[j-1] = new_j_left;

        return true;
    };
};

std::map<std::string, int> node_name_map;
time_t started = time(NULL);

struct penalized_neighbour_compare_t {
    bool operator () (const std::pair<int,neighbour_t> &lhs, const std::pair<int,neighbour_t> &rhs) const;
};


uint16_t read_input(std::vector<node_t*> &nodes, node_t* &start, uint16_t &minimal_price) {
    io::CSVReader<4, io::trim_chars<' '>, io::no_quote_escape<' '> > reader("stdin", std::cin);

    char *start_code = reader.next_line();

    std::string src_code, dest_code;
    uint16_t price;
    uint16_t day;

    uint16_t days_total = 0;

    while(reader.read_row(src_code, dest_code, day, price)) {
        uint16_t src_idx = 0;
        uint16_t dest_idx = 0;

        if (node_name_map.count(src_code) == 0) {
            src_idx = nodes.size();
            node_name_map[src_code] = src_idx;
            node_t *new_node = new node_t(src_code, src_idx);
            nodes.push_back(new_node);
        } else {
            src_idx = node_name_map[src_code];
        }

        if (node_name_map.count(dest_code) == 0) {
            dest_idx = nodes.size();
            node_name_map[dest_code] = dest_idx;
            node_t *new_node = new node_t(dest_code, dest_idx);
            nodes.push_back(new_node);
        } else {
            dest_idx = node_name_map[dest_code];
        }

        route_t *route = new route_t(nodes[src_idx], nodes[dest_idx], price);
        nodes[src_idx]->add_route(day, dest_idx, route);

        if (days_total == 0 || price < minimal_price) minimal_price = price;
        if (day >= days_total) days_total = day + 1;
    }

    // Resize all vectors
    for (auto node_it = nodes.cbegin(); node_it != nodes.cend(); ++node_it) {
        if ((*node_it)->routes.size() < days_total) (*node_it)->routes.resize(days_total);
        for (auto day_it = (*node_it)->routes.begin(); day_it != (*node_it)->routes.end(); ++day_it) {
            if (day_it->size() < days_total) day_it->resize(days_total, NULL);
        }
        
    }

    start = nodes[node_name_map[start_code]];

    return days_total;
}


void cleanup(std::vector<node_t*> nodes) {
    for (auto it_node = nodes.cbegin(); it_node != nodes.cend(); ++it_node) {
        for (auto it_day = (*it_node)->routes.cbegin(); it_day != (*it_node)->routes.cend(); ++it_day) {
            for (auto it_route = it_day->cbegin(); it_route != it_day->cend(); ++it_route) {
                if (*it_route != NULL) delete *it_route;
            }
        }
        delete *it_node;
    }
}

void display(std::vector<route_t *> path, int total_price) {
    std::cout << total_price << std::endl;

    int i = 0;
    for (auto it = path.cbegin(); it != path.cend(); ++it, ++i) {
        std::cout << (*it)->src->name << " " << (*it)->dest->name << " " << i << " " << (*it)->price << std::endl;
    }
}

void depth_search(node_t * start, uint16_t days_total,
                  std::vector<route_t*> &path, int &total_price,
                  bool full_scan) {

    std::set<node_t *> visited_nodes;
    node_t *current_node = start;

    // Full scan variables
    std::vector<route_t*> best_path;
    int best_price = -1;

    std::stack<stack_op_t> stack;
    uint16_t day = 0;
    visited_nodes.insert(start);

    // Preload stack with routes of the first node
    std::set<route_t*, route_ptr_compare_t> ordered_routes;  // Sort routes in set
    for (auto it = start->routes[0].cbegin(); it != start->routes[0].cend(); ++it) {
        if (*it != NULL) ordered_routes.insert(*it);
    }
    for (auto it = ordered_routes.crbegin(); it != ordered_routes.crend(); ++it) {
        stack.push(stack_op_t(FORTH, *it));
    }

    do {
        if (stack.top().op == BACK) {
            total_price -= stack.top().route->price;
            visited_nodes.erase(path.back()->dest);
            path.pop_back();
            day--;
            stack.pop();
        } else {
            day++;
            route_t * this_route = stack.top().route;
            path.push_back(this_route);

            node_t * this_node = this_route->dest;
            visited_nodes.insert(this_node);

            total_price += this_route->price;

            stack.pop();
            
            stack.push(stack_op_t(BACK, this_route));

            if (day == days_total) {
                if (full_scan) {
                    if (best_price == -1 || best_price > total_price) {
                        best_path = path;
                        best_price = total_price;
                        //display(best_path, best_price);
                    }
                } else {
                    break;
                }
            }

            std::set<route_t*, route_ptr_compare_t> ordered_routes;  // Sort routes in set
            for (auto it = this_node->routes[day].cbegin(); it != this_node->routes[day].cend(); ++it) {
                if (*it == NULL) continue;
                if (
                        (day == days_total - 1 && (*it)->dest == start)
                    ||
                        !visited_nodes.count((*it)->dest)
                    ) {

                    ordered_routes.insert(*it);
                }
            }
            for (auto it = ordered_routes.crbegin(); it != ordered_routes.crend(); ++it) {
                stack.push(stack_op_t(FORTH, *it));
            }
        }
    } while (!stack.empty());

    if (day != days_total && !(full_scan && best_price != -1)) {
        std::cerr << "Stack depleted, no circle" << std::endl;
    }

    if (full_scan) {
        path = best_path;
        total_price = best_price;
    }
}


neighbour_t find_best_neighbour(uint16_t days_total,
                                int current_price,
                                std::vector<route_t*> &path,
                                int best_price,
                                semimatrix_t * tabu, semimatrix_t * freq,
                                uint16_t minimal_price) {

    std::set<std::pair<int, neighbour_t>, penalized_neighbour_compare_t> penalized_neighbours;
    neighbour_t best_neighbour = {0,0,0};

    for (uint16_t i = 2; i < days_total - 1; ++i) {
        for (uint16_t j = 1; j < i; ++j) {
            if (i == j) continue;

            int neighbour_price = current_price;
            /* TEST
            int test_price=0;
            for (int t=0; t<days_total; ++t){
                test_price += path[t]->price;
            }
            assert(test_price == neighbour_price);
            */
            
            route_t* old_i_left = path[i - 1];
            route_t* old_i_right = path[i];

            route_t* old_j_left = path[j - 1];
            route_t* old_j_right = path[j];

            node_t* node_i = path[i]->src;
            node_t* node_j = path[j]->src;

            route_t* new_i_left = old_j_left->src->routes[j-1][node_i->idx];
            route_t* new_i_right = node_i->routes[j][old_j_right->dest->idx];

            route_t* new_j_left = old_i_left->src->routes[i-1][node_j->idx];
            route_t* new_j_right = node_j->routes[i][old_i_right->dest->idx];

            if (new_i_left == NULL || new_i_right == NULL || new_j_left == NULL || new_j_right == NULL) {
                // std::cerr << "Not a valid neighbour - route missing" << std::endl;
                continue;
            }

            neighbour_price -= (old_i_right->price + old_j_left->price + old_j_right->price);
            if (i - j > 1) neighbour_price -= old_i_left->price;

            neighbour_price += (new_i_right->price + new_j_left->price + new_j_right->price);
            if (i - j > 1) neighbour_price += new_i_left->price;


            if (tabu->applies(i, j)) {
                //std::cerr << "Neighbour in tabu" << std::endl;
                if (neighbour_price < best_price) {
                    //std::cerr << "Tabu cancelled - Aspiration criteria met" << std::endl;
                } else {
                    continue;
                }
            }

            penalized_neighbours.insert(std::pair<int, neighbour_t>(neighbour_price + minimal_price * freq->get(i, j), neighbour_t(i, j, neighbour_price)));

            if (best_neighbour.i == 0 || best_neighbour.price > neighbour_price) {
                best_neighbour.i = i;
                best_neighbour.j = j;
                best_neighbour.price = neighbour_price;

                /* TEST
                int test_price=0;
                for (int t=0; t<days_total; ++t){
                    if (t == i) test_price += new_i_right->price;
                    else if (t == j - 1) test_price += new_j_left->price;
                    else if (t == j) test_price += new_j_right->price;
                    else if (t == i - 1) test_price += new_i_left->price;
                    else test_price += path[t]->price;
                }

                if (test_price != neighbour_price) {
                    std::cerr << "Fuck up " << neighbour_price << " vs " << test_price << std::endl;
                    std::cerr << "i=" << i << " j=" << j << std::endl;
                    assert(false);
                }*/

            }


        }
    }

    //std::cerr << "Returning best neighbour with price " << best_neighbour.price << std::endl;
    //std::cerr << "i=" << best_neighbour.i << " j=" << best_neighbour.j << std::endl;
    
    if (best_neighbour.i == 0) {
        // No applicable neighbours
        return best_neighbour;
    }

    if (best_neighbour.price < best_price) {
        return best_neighbour;
    } else {
        /*
        std::cerr << "No interesting neighbour, picking by frequency-penalized price" << std::endl;
        std::cerr << penalized_neighbours.cbegin()->second.i << std::endl;
        std::cerr << penalized_neighbours.crbegin()->second.j << std::endl;
        */
        return penalized_neighbours.cbegin()->second;
    }


}

void recalculate_price(std::vector<route_t*> path, int * price) {
    *price = 0;
    for (auto it = path.cbegin(); it != path.cend(); ++it) {
        *price += (*it)->price;
    }
}


void tabu_search(node_t * start, uint16_t days_total, std::vector<route_t*> &best_path,
                 int &best_price, uint16_t minimal_price) {

    std::vector<route_t*> current_path = best_path;
    int current_price = best_price;

    semimatrix_t tabu(days_total - 1, days_total - 1);
    semimatrix_t freq(days_total - 1, days_total - 1);

    int iter_since_improvement = 0;

    while (difftime(time(NULL), started) < 29) {
        neighbour_t neighbour = find_best_neighbour(days_total, current_price,
                                                    current_path, best_price,
                                                    &tabu, &freq,
                                                    minimal_price);

        if (neighbour.i != 0) {
            neighbour.apply(current_path);
            tabu.set(neighbour.i, neighbour.j);
            freq.inc(neighbour.i, neighbour.j);
            current_price = neighbour.price;
            if (neighbour.price < best_price) {
                iter_since_improvement = 0;
                best_path = current_path;
                best_price = current_price;
                //display(current_path, neighbour.price);
            } else {
                iter_since_improvement++;
            }
        } else {
            //std::cerr << "No applicable neighbour" << std::endl;
        }

        /*
        if (iter_since_improvement > 400) {
            bool applied = false;
            while (!applied && difftime(time(NULL), started) < 29) {
                neighbour_t neighbour(0,0,0);
                freq.get_minimum(&neighbour.i, &neighbour.j);
                applied = neighbour.try_apply(current_path);
                //if (!applied) std::cout << "not applicable" << std::endl;
            }
            if (applied) {
                tabu.clear();
                freq.clear();
                iter_since_improvement++;
                recalculate_price(current_path, &current_price);
            }
        }
        */
    }

    //display(best_path, best_price);

}


int main(int argc, char **argv) {

    std::vector<node_t*> nodes;
    node_t* start;
    uint16_t minimal_price = 0;
    //std::cerr << "Loading " << std::endl;
    uint16_t days_total = read_input(nodes, start, minimal_price);

    //std::cerr << "Loading done" << std::endl;

    std::vector<route_t *> path;
    int total_price = 0;

    bool full_scan = days_total < 20;
    full_scan = false;

    depth_search(start, days_total, path, total_price, full_scan);
    //display(path, total_price);

    if (!full_scan) {
        tabu_search(start, days_total, path, total_price, minimal_price);
    }

    display(path, total_price);

    cleanup(nodes);

    return 0;
}

bool route_ptr_compare_t::operator () (const route_t* const &lhs, const route_t* const &rhs) const {
    return lhs->price < rhs->price;
}

bool penalized_neighbour_compare_t::operator () (const std::pair<int,neighbour_t> &lhs, const std::pair<int,neighbour_t> &rhs) const {
    return lhs.first < rhs.first;
}
