#include <iostream>
#include <chrono>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <functional>

using namespace std;


/**
 * Source Code for the experiment comparing the performance of foreign keys vs pointer-based star join.
 *
 **/

/** A class to represent a dimension object, i.e. a tuple of a dimension table. */
class DimensionObject {
public:
    size_t id_;
    string info_;

    DimensionObject(const size_t id, string info) : id_(id), info_(std::move(info)) {
    }
};

/*  A class to represent a tuple in the fact table */
template<typename T>
class FactTableObject {
public:
    vector<T> relations_;

    explicit FactTableObject(vector<T> relations) : relations_(std::move(relations)) {
    }
};

// the number of tuples in each dimension table:
constexpr int number_of_tuples_in_dimension_tables = 100;
//vary number of dimension tables up to <max_number_of_dimension_tables>:
constexpr int max_number_of_dimension_tables = 10;
// the number of repetitions for each experiment:
constexpr int number_of_repetitions = 10;
// the number of tuples in the fact table:
constexpr int number_of_tuples_in_fact_table = 50'000'000;

// a dimension table is a vector of dimension objects:
using DimensionTable = std::vector<DimensionObject>;

// a hash map mapping from the id to the object (through a pointer):
using DimensionTableIndex = std::unordered_map<size_t, shared_ptr<DimensionObject> >;

template<typename FactTableObject, typename FactTableAttribute>
std::vector<FactTableObject> get_fact_table(const size_t number_of_dimension_tables,
                                            std::vector<FactTableAttribute> &current_tuple,
                                            const std::function<void(size_t i, size_t j)> &add_entry_to_tuple) {
    std::vector<FactTableObject> fact_table;
    auto tuples_in_fact_table = 0;

    /* Recursive lambda function to create a fact table. In each step we pass recursively extend a stack with entries from the dimension tables
       For example, we start with the empty stack [], which is then passed to dimension table (DT) 0. In DT 0
       0, we copy add a pointer to the first tuple from DT0, such that we have the stack [pointer_to_tuple_0_from_DT0].
       Afterward, we recursively move to DT1, which then again adds its own first tuple to the stack,
       such that we have [pointer_to_tuple_0_from_DT0, pointer_to_tuple_0_from_DT1] in the stack. This is repeated until we get
        to the last DTn, we obtain a full tuple stack [pointer_to_tuple_0_from_DT0, pointer_to_tuple_0_from_DT1, ...,
        pointer_to_tuple_0_from_DTn] in the stack, which we then copy and add to the fact table. Afterward we remove the most recent
        addition to the stack. This procedure is repeated for every possible
        tuple for each DT, until we obtain number_of_tuples_in_fact_table.
     */
    auto rec_pointer = [&](const std::size_t dimension_table_index, auto rec_function) -> void {
        /* Go through each possible tuples in the dimension table */
        for (size_t tuple_index = 0; tuple_index < number_of_tuples_in_dimension_tables; tuple_index++) {
            if (tuples_in_fact_table >= number_of_tuples_in_fact_table) {
                /* If the upperbound of tuples has been met, do not produce any more tuples */
                return;
            }

            /* Add the pointer to the current dimension_table entry to the entry in the fact_table tuple */
            add_entry_to_tuple(dimension_table_index, tuple_index);

            if (dimension_table_index < number_of_dimension_tables - 1) {
                /* Last dimension table not met yet, go to next dimension table */
                rec_function(dimension_table_index + 1, rec_function);
            } else {
                /* Full fact_table tuple has been created, add it to the fact table and increase counter */
                fact_table.emplace_back(current_tuple);
                tuples_in_fact_table++;
            }
            current_tuple.pop_back();
        }
    };
    rec_pointer(0, rec_pointer);
    return fact_table;
}


void evaluate_for_n_dimensions(const size_t n, ofstream &output, ofstream &output_build) {
    /**
     * This function evaluates the performance of the foreign key vs pointer-based star join for n dimensions.
     * It creates n dimension tables with <number_of_tuples_in_dimension_tables> tuples each and then creates a fact table
     * with <number_of_tuples_in_fact_table> tuples. The function then measures the time taken to access the dimension
     * objects through foreign keys and pointers.
     */
    /// Create a vector of dimension tables and a vector of dimension indexes, bith combined form the star schema
    std::vector<DimensionTable> dimensions;
    std::vector<DimensionTableIndex> dimension_indexes;

    cout << "Running Experiment for " << n << " Dimensions!\n";

    // (1.) create dimension tables and dimension indexes:

    /* Create <n> dimension tables with <number_of_tuples> tuples each */
    for (size_t i = 0; i < n; i++) {
        // create instance of dimension and add it to the vector:
        dimensions.emplace_back();
        // create instance of dimension index and add it to the vector:
        dimension_indexes.emplace_back();
        // Create <number_of_tuples> tuples for each dimension:
        for (size_t j = 0; j < number_of_tuples_in_dimension_tables; j++) {
            // create instance of RelationObject and add it to the vector:
            // i.e. this calls the constructor of the DimensionObject class:
            dimensions[i].emplace_back(j, to_string(j));
            // create a pointer to the object and add it to the index:
            dimension_indexes[i][j] = make_shared<DimensionObject>(dimensions[i][j]);
        }
    }
    for (size_t i = 0; i < number_of_repetitions; i++) {
        // (2.) create te fact tables: create the cartesian product of the dimension tables until we reach the number of
        // tuples in the fact table:

        // (2a.) Create relationship storing foreign keys to each dimension (RM):
        using ForeignKeyTableObject = FactTableObject<size_t>;
        using ForeignKeyRelation = std::vector<ForeignKeyTableObject>;
        std::vector<size_t> current_foreign_key_tuple;
        auto add_foreign_key_to_tuple = [&](const size_t dimension_table_index, const size_t tuple_index) {
            current_foreign_key_tuple.emplace_back(dimensions[dimension_table_index][tuple_index].id_);
        };

        auto start = chrono::high_resolution_clock::now();
        const ForeignKeyRelation fact_table_RM = get_fact_table<ForeignKeyTableObject>(
            n, current_foreign_key_tuple, add_foreign_key_to_tuple);
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        output_build << n << ",foreign," << duration.count() / 1E6 << "\n";

        // (2b.) Create relationship storing pointers to each dimension (RTM):
        using PointerFactTableObject = FactTableObject<shared_ptr<DimensionObject> >;
        using PointerFactTable = std::vector<PointerFactTableObject>;
        std::vector<shared_ptr<DimensionObject> > current_pointer_tuple;
        auto add_pointer_to_tuple = [&](const size_t dimension_table_index, const size_t tuple_index) {
            current_pointer_tuple.emplace_back(dimension_indexes[dimension_table_index][tuple_index]);
        };

        start = chrono::high_resolution_clock::now();
        const PointerFactTable fact_table_RTM = get_fact_table<PointerFactTableObject>(
            n, current_pointer_tuple, add_pointer_to_tuple);
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        output_build << n << ",pointer," << duration.count() / 1E6 << "\n";

        cout << fact_table_RM.size() << "\n" << fact_table_RTM.size() << "\n";

        // (3.) Now evaluate the different star join variants
        // (3a.) Measure performance of RM (foreign keys):
        //for (size_t i = 0; i < number_of_repetitions; i++) {
        start = chrono::high_resolution_clock::now();
        /* Go through tuple in the fact table */
        for (auto &rel_object: fact_table_RM) {
            for (size_t j = 0; j < n; j++) {
                /* Access the index to obtain the dimension table object */
                auto dimension_table_object = *dimension_indexes[j][rel_object.relations_[j]];
            }
        }
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        output << n << ",foreign," << duration.count() / 1E6 << "\n";
        cout << "Time taken without pointers: " << duration.count() / 1E6 << " ms" << endl;
        cout << std::flush;
        //}

        // (3b.) Measure performance of RTM (pointers):
        //for (size_t i = 0; i < number_of_repetitions; i++) {
        start = chrono::high_resolution_clock::now();
        /* Go through tuple in the fact table */
        for (auto &rel_object: fact_table_RTM) {
            for (size_t j = 0; j < n; j++) {
                /* Access the pointer to obtain the dimension table object */
                auto relation_object = *rel_object.relations_[j];
            }
        }
        end = chrono::high_resolution_clock::now();
        duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        output << n << ",pointer," << duration.count() / 1E6 << "\n";
        cout << "Time taken with pointers: " << duration.count() / 1E6 << " ms" << endl;
        cout << std::flush;
    }
}

int main() {
    // one file for the results of the star-join runtimes:
    std::ofstream output("./results/results.csv");
    // one file for the results of the build times:
    std::ofstream output_build("./results/results_build.csv");
    // write headers:
    output << "exp,type,time" << "\n";
    output_build << "exp,type,time" << "\n";

    // main loop for the experiments:
    for (size_t i = 2; i <= max_number_of_dimension_tables; i++) {
        // skip experiment if the number of tuples in the Cartesian product is smaller than the number of tuples in
        // the fact table:
        if (pow(number_of_tuples_in_dimension_tables, i) < number_of_tuples_in_fact_table) {
            cout << "Not enough tuples in the Cartesian product for " << i << " relations!\n";
            continue;
        }
        // build the star schema and evaluate the performance:
        evaluate_for_n_dimensions(i, output, output_build);
    }
    return 0;
}

/* Possible Command for Compilation: clang++ -std=c++20 -O2 -o build/benchmark benchmark.cpp */
