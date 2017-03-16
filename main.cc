// vim: let g:syntastic_cpp_compiler_options=' -std=c++0x '

#include <iostream>
#include <map>
#include <vector>
#include <string>
#include <set>
#include <unordered_set>
#include <stack>
#include "csv.h"

struct route_t;

struct route_ptr_compare_t {
    bool operator () (const route_t* const &lhs, const route_t* const &rhs) const;
};

struct node_t {
    node_t(std::string name) : name(name) {};
    route_t * get_route(uint16_t day, std::string dest_name) {

        auto routes_for_day_it = routes.find(day);
        if (routes_for_day_it == routes.end()) return NULL;

        auto route_it = routes_for_day_it->second.find(dest_name);
        if (route_it == routes_for_day_it->second.end()) return NULL;
        
        return route_it->second;
    };

    std::string name;
    //std::map<int, std::set<route_t*, route_ptr_compare_t> > routes;
    std::map<uint16_t, std::map<std::string, route_t*> > routes;
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

struct tabu_t {
    tabu_t(uint16_t node_count) : node_count(node_count), current_max(0), threshold(40) {
        tabu_map = new int*[node_count];
        for (uint16_t i = 1; i < node_count; ++i) {
            tabu_map[i] = new int[i - 1];
            for (uint16_t j = 0; j < i -1; j++) {
                tabu_map[i][j] = 0;
            }
        }
    }
    ~tabu_t() {
        for (uint16_t i = 0; i < node_count; ++i) {
            delete tabu_map[i];
        }
        delete tabu_map;
    }

    void set(uint16_t node1, uint16_t node2) {
        normalize(&node1, &node2);

        current_max++; // TODO handle overflow
        tabu_map[node1][node2] = current_max;
    }

    bool applies(uint16_t node1, uint16_t node2) {
        normalize(&node1, &node2);

        return tabu_map[node1][node2] > current_max - threshold;
    }

    void inc(uint16_t node1, uint16_t node2) {
        normalize(&node1, &node2);
        tabu_map[node1][node2]++;
    }

    // TODO inline
    void normalize(uint16_t * node1, uint16_t * node2) {
        if (*node2 > *node1) {
            uint16_t temp;
            temp = *node1;
            *node1 = *node2;
            *node2 = temp;
        }
    }

    int **tabu_map;
    uint16_t node_count;
    int current_max;
    int threshold;
};






uint16_t read_input(std::map<std::string, node_t*> &nodes, node_t* &start) {
    io::CSVReader<4, io::trim_chars<' '>, io::no_quote_escape<' '> > reader("stdin", std::cin);
    char *start_code = reader.next_line();


    std::string src_code, dest_code;
    uint16_t price;
    uint16_t day;

    uint16_t days_total = 0;

    while(reader.read_row(src_code, dest_code, day, price)) {
        if (nodes.count(src_code) == 0) {
            node_t *new_node = new node_t(src_code);
            nodes.insert(std::pair<std::string, node_t*>(src_code, new_node));
        }
        if (nodes.count(dest_code) == 0) {
            node_t *new_node = new node_t(dest_code);
            nodes.insert(std::pair<std::string, node_t*>(dest_code, new_node));
        }

        route_t *route = new route_t(nodes[src_code], nodes[dest_code], price);
        nodes[src_code]->routes[day][dest_code] = route;

        if (day >= days_total) days_total = day + 1;
    }

    start = nodes[start_code];

    return days_total;
}


void cleanup(std::map<std::string, node_t*> nodes) {
    for (auto it_node = nodes.cbegin(); it_node != nodes.cend(); ++it_node) {
        for (auto it_day = it_node->second->routes.cbegin(); it_day != it_node->second->routes.cend(); ++it_day) {
            for (auto it_route = it_day->second.cbegin(); it_route != it_day->second.cend(); ++it_route) {
                delete it_route->second;
            }
        }
        delete it_node->second;
    }
}

void display(std::vector<route_t *> path, int total_price) {
    std::cout << total_price << std::endl;

    int i = 0;
    for (auto it = path.cbegin(); it != path.cend(); ++it, ++i) {
        std::cout << (*it)->src->name << " " << (*it)->dest->name << " " << i << " " << (*it)->price << std::endl;
    }
}

void depth_search(node_t * start, uint16_t days_total, std::vector<route_t*> &path, int &total_price) {
    std::set<node_t *> visited_nodes;
    node_t *current_node = start;

    std::stack<stack_op_t> stack;
    uint16_t day = 0;
    visited_nodes.insert(start);

    // Preload stack with routes of the first node
    std::set<route_t*, route_ptr_compare_t> ordered_routes;  // Sort routes in set
    for (auto it = start->routes[0].cbegin(); it != start->routes[0].cend(); ++it) {
        ordered_routes.insert(it->second);
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
                break;
            }

            std::set<route_t*, route_ptr_compare_t> ordered_routes;  // Sort routes in set
            for (auto it = this_node->routes[day].cbegin(); it != this_node->routes[day].cend(); ++it) {
                if (
                        (day == days_total - 1 && it->second->dest == start)
                    ||
                        !visited_nodes.count(it->second->dest)
                    ) {

                    ordered_routes.insert(it->second);
                }
            }
            for (auto it = ordered_routes.crbegin(); it != ordered_routes.crend(); ++it) {
                stack.push(stack_op_t(FORTH, *it));
            }
        }
    } while (!stack.empty());

    std::cerr << day << std::endl;
    if (day != days_total) {
        std::cerr << "Stack depleted" << std::endl;
    }
}


void find_best_neighbour(uint16_t days_total, int &current_price, std::vector<route_t*> &path) {
    uint16_t best_i = 0;
    uint16_t best_j = 0;

    int best_neighbour_price = 0;

    for (uint16_t i = 2; i < days_total - 1; ++i) {
        for (uint16_t j = 1; j < i; ++j) {
            if (i == j) continue;

            int neighbour_price = current_price;

            route_t* old_i_left = path[i - 1];
            route_t* old_i_right = path[i];

            route_t* old_j_left = path[j - 1];
            route_t* old_j_right = path[j];

            node_t* node_i = path[i]->src;
            node_t* node_j = path[j]->src;

            route_t* new_i_left = old_j_left->src->get_route(j-1, node_i->name);
            route_t* new_i_right = node_i->get_route(j, old_j_right->dest->name);

            route_t* new_j_left = old_i_left->src->get_route(i-1, node_j->name);
            route_t* new_j_right = node_j->get_route(i, old_i_right->dest->name);

            if (new_i_left == NULL || new_i_right == NULL || new_j_left == NULL || new_j_right == NULL) {
                std::cerr << "Not a valid neighbour - route missing" << std::endl;
                continue;
            }

            neighbour_price -= (old_i_left->price + old_i_right->price + old_j_left->price + old_j_right->price);
            neighbour_price += (new_i_left->price + new_i_right->price + new_j_left->price + new_j_right->price);

            if (best_i == 0 || best_neighbour_price > neighbour_price) {
                best_i = i;
                best_j = j;
                best_neighbour_price = neighbour_price;
            }

        }
    }

}


void tabu_search(node_t * start, uint16_t days_total, std::vector<route_t*> &path, int &total_price) {
    int best_price = total_price;
    tabu_t tabu(days_total);
    tabu_t freq(days_total);

    find_best_neighbour(days_total, total_price, path);
}


int main(int argc, char **argv) {

    std::map<std::string, node_t*> nodes;
    node_t* start;
    std::cerr << "Loading " << std::endl;
    uint16_t days_total = read_input(nodes, start);
    
    std::cerr << "Loading done" << std::endl;

    std::vector<route_t *> path;
    int total_price = 0;

    depth_search(start, days_total, path, total_price);
    tabu_search(start, days_total, path, total_price);

    display(path, total_price);

    cleanup(nodes);

    return 0;
}

bool route_ptr_compare_t::operator () (const route_t* const &lhs, const route_t* const &rhs) const {
    return lhs->price < rhs->price;
}
/*
    // Bootstrap naive path
    std::vector<route_t *> path;
    std::set<node_t *> visited_nodes;
    node_t *current_node = start;
    int total_price = 0;

    visited_nodes.insert(start);
    for (uint16_t day = 0; day < days_total; ++day) {
        bool route_appended = false;
        for (auto it = current_node->routes[day].cbegin(); it != current_node->routes[day].cend(); ++it) {
            if (
                    (day == days_total - 1 && (*it)->dest == start)
                ||
                    !visited_nodes.count((*it)->dest)
                ) {

                path.push_back((*it));
                current_node = (*it)->dest;
                visited_nodes.insert(current_node);
                total_price += (*it)->price;
                route_appended = true;
                break;
            }
        }
        if (!route_appended) {
            std::cout << "No straight way, man! :)" << std::endl;
            break;
        }
    }
    */
