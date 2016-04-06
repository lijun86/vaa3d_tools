//
//  Composite.cpp
//  BigNeuron Consensus Builder
//
//  Created by Todd Gillette on 5/29/15.
//  Copyright (c) 2015 Todd Gillette. All rights reserved.
//

#include <stdio.h>
#include <stack>
#include <set>
#include <math.h>
#include <tgmath.h>
#include <algorithm>
#include "Composite.h"

/**
 * Below are classes associated directly with the Composite
 */

/******************************
 ** CompositeBranchContainer **
 ******************************/
CompositeBranchContainer::CompositeBranchContainer(){
    cbc_composite = nullptr;
    cbc_parent = nullptr;
    combined_connection_weight = 0;
    best_connection = nullptr;
    segment_reversed = false;
    bc_confidence = 0;
    confidence_denominator = 0;
};
CompositeBranchContainer::CompositeBranchContainer(NeuronSegment *segment, Composite *composite){
    cbc_composite = composite;
    bc_segment = segment;
    if (composite) composite->add_branch(this);
    cbc_parent = nullptr;
    combined_connection_weight = 0;
    best_connection = nullptr;
    segment_reversed = false;
    bc_confidence = 0;
    confidence_denominator = 0;
    subtree_markers = segment->markers.size();
};
/**
 * Creates a CompositeBranchContainer that is a copy of the input branch
 *   - Deep copy of NeuronSegment
 **/
CompositeBranchContainer::CompositeBranchContainer(CompositeBranchContainer *branch){
    copy_from(branch, branch->get_composite());
};
CompositeBranchContainer::CompositeBranchContainer(CompositeBranchContainer *branch, Composite *composite){
    copy_from(branch, composite);
};
void CompositeBranchContainer::copy_from(CompositeBranchContainer *branch, Composite *composite){
//    printf("CBC:copy_from Enter, copy from branch %p\n",branch);
    NeuronSegment *segment_copy = copy_segment_markers(branch->get_segment());
    set_segment(segment_copy);

    //    printf("CBC:copy_from 0l best_connection %p\n",get_best_connection_weight().first);
    best_connection = nullptr;
    parent_connections = branch->copy_connections();
    calculate_best_connection();

//    printf("CBC:copy_from 1\n");
    cbc_composite = composite;
    if (composite) composite->add_branch(this);
    
//    printf("CBC:copy_from 2\n");
    segment_reversed = branch->is_segment_reversed();
    branch_matches = branch->get_branch_matches();
//    printf("CBC:copy_from 3\n");
    bc_confidence = branch->get_summed_confidence();
//    printf("CBC:copy_from 4\n");
    confidence_denominator = branch->get_confidence_denominator();
    cbc_parent = nullptr;
    
    combined_connection_weight = branch->get_combined_connection_weight();
};
ConnectionPtrSet CompositeBranchContainer::copy_connections() const{
    ConnectionPtrSet new_connections;
//    printf("CBC:copy_connections 1\n");

    for (Connection *orig_con : parent_connections){
//        printf("CBC:copy_from connection %p, has %i recons\n",orig_con, orig_con->get_reconstructions().size());
        new_connections.insert(new Connection(orig_con));
    }
    return new_connections;
};




CompositeBranchContainer::~CompositeBranchContainer(){
    if (cbc_composite){
        cbc_composite->remove_branch(this);
    }
    /*
    set_parent(nullptr);
    remove_children();
    
    printf("Removing connections\n");

    for (Connection * connection : parent_connections){
        printf("Removing connections, parent %p\n",connection->get_parent());
        if (connection->get_parent()){
            connection->get_parent()->remove_child_connection(connection);
            printf("Removed parent's child connection\n");
        }
        delete connection;
    }
     */
};

void CompositeBranchContainer::prepare_for_deletion(){
    set_parent(nullptr);
    remove_children();
    
    for (Connection *connection : parent_connections){
        if (connection->get_parent()){
            connection->get_parent()->remove_child_connection(connection);
        }
    }
}

std::set<Connection *> CompositeBranchContainer::deleted_connections;
void CompositeBranchContainer::delete_connections(){
    //printf("Deleting %i connections for branch %p\n",parent_connections.size(),this);
    for (Connection *connection : parent_connections){
        if (this != connection->get_child()){
            printf("WARNING: Connection PROBLEM! current branch %p, connection %p, child %p, parent %p\n",this, connection, connection->get_child(), connection->get_parent());
        }
        
     /*   printf("Deleting connection %p\n",connection);
        printf("%p %p\n", connection->get_parent(), connection->get_child());
      */
        if (deleted_connections.find(connection) != deleted_connections.end()){
            printf("ALREADY DELETED!!!\n");
        }else{
            deleted_connections.insert(connection);
            
            delete connection;
        }
    }
    //printf("Done deleting connections\n");
}


void CompositeBranchContainer::add_branch_match(BranchContainer * match){
    add_branch_match(match, true);
};
void CompositeBranchContainer::add_branch_match(BranchContainer * match, bool match_forward){
    branch_matches.push_back(match);
    match_forward_map[match] = match_forward;
//    printf("add_branch_match, current summed conf %f, to be added %f\n",bc_confidence,match->get_confidence());
//    printf("add_branch_match conf before %f\n",bc_confidence);
    bc_confidence += match->get_confidence();
//    printf("add_branch_match conf after %f\n",bc_confidence);
    confidence_denominator += match->get_confidence();
//    printf("add_branch_match confidence_denominator %f\n",confidence_denominator);
};
bool CompositeBranchContainer::is_match_forward(BranchContainer * match){
    return match_forward_map[match];
};
void CompositeBranchContainer::reset_confidence(){
    bc_confidence = 0;
    confidence_denominator = 0;
}
void CompositeBranchContainer::add_branch_miss(double confidence){
    confidence_denominator += confidence;
};
void CompositeBranchContainer::set_segment_reversed(bool segment_reversed){
    this->segment_reversed = segment_reversed;
};
void CompositeBranchContainer::reverse_segment(){
    segment_reversed = !segment_reversed;
    std::reverse(bc_segment->markers.begin(),bc_segment->markers.end());
};
bool CompositeBranchContainer::is_segment_reversed(){
    return segment_reversed;
};

void CompositeBranchContainer::set_parent(CompositeBranchContainer *parent, bool follow, BranchEnd branch_end, BranchEnd parent_end){
    if (cbc_parent && follow){
        cbc_parent->remove_child(this, false);
    }
    cbc_parent = parent;
    if (parent)
        cbc_parent->add_child(this);
};
void CompositeBranchContainer::add_child(CompositeBranchContainer *child){
    if (child){
        cbc_children.insert(child);
        std::vector<NeuronSegment *>::iterator position = std::find(bc_segment->child_list.begin(), bc_segment->child_list.end(), child->get_segment());
        if (position == bc_segment->child_list.end()){
//            printf("pushing segment starting at %f %f into list of children of segment starting at %f %f\n",child->get_segment()->markers[0]->x,child->get_segment()->markers[0]->y,get_segment()->markers[0]->x,get_segment()->markers[0]->y);
            bc_segment->child_list.push_back(child->get_segment());
        }
        if (child->get_parent() != this){
            child->set_parent(this);
        }
        subtree_markers += child->get_subtree_markers();
    }
};
void CompositeBranchContainer::remove_child(CompositeBranchContainer *child, bool follow){
    if (child && child->get_parent() != this){
        printf("WARNING: CBC::remove_child %p but it's not a child of branch %p\n",child,this);
    }
    //if (child && child->get_parent() == this){
    if (child && cbc_children.find(child) != cbc_children.end()){
        if (follow) child->set_parent(nullptr, false);

        int num_children_before = cbc_children.size();
        cbc_children.erase(child);
        if (cbc_children.size() < num_children_before) subtree_markers -= child->get_subtree_markers();

        NeuronSegment *child_seg = child->get_segment();
        if (child_seg)
            bc_segment->child_list.erase(std::remove(bc_segment->child_list.begin(), bc_segment->child_list.end(), child_seg),bc_segment->child_list.end());
        
        /*
        std::vector<NeuronSegment *>::iterator position = std::find(bc_segment->child_list.begin(), bc_segment->child_list.end(), child->get_segment());
        printf("found child's segment in segment child_list? %i \n",position != bc_segment->child_list.end());
        if (position != bc_segment->child_list.end()) // == vector.end() means the element was not found
            bc_segment->child_list.erase(position);
         */
    }else{
        printf("ERROR in remove_child? child %p not in children of %p\n",child, this);
    }
};
void CompositeBranchContainer::remove_children(){
    std::set<CompositeBranchContainer *> tmp_children = cbc_children;
    for (CompositeBranchContainer *child : tmp_children){
        remove_child(child);
    }
};

// Returns new branch above the split; Transfers all connections on the TOP end to the new branch
CompositeBranchContainer * CompositeBranchContainer::split_branch(std::size_t const split_point){

    // Get original segment
    NeuronSegment *orig_seg = bc_segment;
    
    if (split_point <= 0 || split_point > orig_seg->markers.size()-1){
        printf("ERROR! split_point is %i, segment length is %i\n",split_point,orig_seg->markers.size());
    }
    
    // Create new segment above split
    NeuronSegment *new_seg_before = new NeuronSegment();
    std::size_t const split_point_t = split_point;
    new_seg_before->markers = std::vector<MyMarker *>(orig_seg->markers.begin(), orig_seg->markers.begin() + split_point_t);
    //new_seg_below(orig_seg->markers.begin() + split_point_t, orig_seg->markers.end());
    orig_seg->markers.erase(orig_seg->markers.begin(), orig_seg->markers.begin() + split_point_t);
    subtree_markers -= split_point;

    /*
    printf("Markers in new segment before\n");
    for (MyMarker * mrk : new_seg_before->markers){
        printf("%f %f %f\n",mrk->x,mrk->y,mrk->z);
    }
    printf("Markers in segment after\n");
    for (MyMarker * mrk : orig_seg->markers){
        printf("%f %f %f\n",mrk->x,mrk->y,mrk->z);
    }
*/
    // Create new branch above split
    CompositeBranchContainer *branch_before = new CompositeBranchContainer(new_seg_before, cbc_composite);

    // Transfer TOP or BOTTOM connections to new branch_before (depending on whether branch is reversed at the moment)
    // Update connections, and add contributing reconstructions to new connection
    std::set<Connection *> tmp_connections;
    for (Connection *connection : parent_connections) {
        tmp_connections.insert(connection);
    }
//    printf("Branch %p, branch_before %p\n",this,branch_before);
    for (Connection *connection : tmp_connections){
        // Connections at the TOP of the segment need to be transfered to the new segment above the split
        // OR if the segment is reversed then segments at the BOTTOM need to be transfered
        if (segment_reversed ^ connection->get_child_end() == TOP){
            remove_connection(connection);
            connection->set_child(branch_before); // This will also take care of parent's child connection (top_child_connections & bottom_child_connections)
            branch_before->add_connection(connection);
        }
    }
    
    if (segment_reversed){
        // Transfer child connections that are at the BOTTOM
        for (Connection *connection : bottom_child_connections){
            connection->set_parent(branch_before);
            branch_before->add_child_connection(connection);
        }
        bottom_child_connections.clear();
    }else{
        // Transfer child connections that are at the TOP
        for (Connection *connection : top_child_connections){
            connection->set_parent(branch_before);
            branch_before->add_child_connection(connection);
        }
        top_child_connections.clear();
    }

    // Create connection between branches
    BranchEnd child_end, parent_end;
    Connection *new_connection;
    for (BranchContainer *r_branch : branch_matches){
        //printf("Creating connections given branch_matches; segment_reversed %i, match_forward %i\n",segment_reversed,match_forward_map[r_branch]);
        if (segment_reversed){
            if (match_forward_map[r_branch])
                this->add_connection(BOTTOM, branch_before, TOP, r_branch->get_reconstruction(), r_branch->get_confidence());
                //branch_before->add_connection(TOP, this, BOTTOM, r_branch->get_reconstruction(), r_branch->get_confidence());
            else
                branch_before->add_connection(TOP, this, BOTTOM, r_branch->get_reconstruction(), r_branch->get_confidence());
                //this->add_connection(BOTTOM, branch_before, TOP, r_branch->get_reconstruction(), r_branch->get_confidence());
        }else{
            if (match_forward_map[r_branch])
                this->add_connection(TOP, branch_before, BOTTOM, r_branch->get_reconstruction(), r_branch->get_confidence());
            else
                branch_before->add_connection(BOTTOM, this, TOP, r_branch->get_reconstruction(), r_branch->get_confidence());
        }
    }

    // Split matches [#Consider updating registration bins wherever split_branch is called, though probably unnecessary since these branches have already been matched and won't be compared again]
    branch_before->reset_confidence();
    for (BranchContainer *match : branch_matches){
        // Insert new branch between current and parent branch to match the new composite branch

        BranchContainer *match_before_split = match->split_branch();
        match_before_split->set_composite_match(branch_before);
        branch_before->add_branch_match(match_before_split, match_forward_map[match]);
        match_forward_map[match_before_split] = match_forward_map[match];
        
        // #CONSIDER: Is there any need to also split up NeuronSegments of each Reconstruction?
        // For now we won't as it would require pointers/maps between reconstruction and composite markers, or at least vectors of gaps
        // (which should be rare given the resampling that must occur at the start)
        // PLAN: composite contains map from each marker to vector of matched markers (could be a pair, with the second element being BranchContainer* or branch confidence)
        //      This would allow for determination of the split point AND for calculation of variance of each marker
    }
    
    // Add misses to new composite branch before split
    branch_before->add_branch_miss(confidence_denominator - branch_before->get_confidence_denominator());

    //printf("CBC::split_branch complete, branch_before conf %f, denom %f\n", branch_before->get_summed_confidence(), branch_before->get_confidence_denominator());

    return branch_before;
};


Composite * CompositeBranchContainer::get_composite(){
    return cbc_composite;
};
double CompositeBranchContainer::get_summed_confidence() const{
    return bc_confidence;
};
double CompositeBranchContainer::get_confidence() {
    return (double)bc_confidence / (double)confidence_denominator;
};
double CompositeBranchContainer::get_connection_confidence() const{
    get_connection_confidence(best_connection);
}
double CompositeBranchContainer::get_connection_confidence(Connection *connection) const{
    if (parent_connections.find(connection) == parent_connections.end())
        return 0;
    return connection->get_confidence() / confidence_denominator;
};

CompositeBranchContainer* CompositeBranchContainer::get_parent(){
    return cbc_parent;
};
std::set<CompositeBranchContainer*> CompositeBranchContainer::get_children(){
    return cbc_children;
};
std::set<Reconstruction *> CompositeBranchContainer::get_reconstructions(){
    std::set<Reconstruction *> reconstructions;
    for (BranchContainer *branch : branch_matches){
        reconstructions.insert(branch->get_reconstruction());
    }
    return reconstructions;
};

/*
void CompositeBranchContainer::add_connection(BranchEnd child_end, CompositeBranchContainer * parent, BranchEnd parent_end, double confidence){
    Connection * connection = new Connection(this, child_end, parent, parent_end);
    connection->set_confidence(confidence);
    add_connection(connection);
};
 */
void CompositeBranchContainer::add_connection(BranchEnd child_end, CompositeBranchContainer *parent, BranchEnd parent_end, Reconstruction *reconstruction, double confidence){

    if (confidence == 0 && reconstruction) confidence = reconstruction->get_confidence();
    Connection *connection = new Connection(this, child_end, parent, parent_end, reconstruction, confidence);
    int return_stat = add_connection(connection);
    if (return_stat == 1) delete connection;
};

// Adds confidence for given connection, or creates new connection if it does not already exist
int CompositeBranchContainer::add_connection(Connection *connection){
    if (this == connection->get_child()){
        combined_connection_weight += connection->get_confidence();

//        ConnectionPtrSet::iterator it = parent_connections.find(connection);
        Connection *existing_conn = nullptr;

        for (Connection *test_conn : parent_connections){
            if (test_conn->get_parent() == connection->get_parent() && test_conn->get_parent_end() == connection->get_parent_end() && test_conn->get_child_end() == connection->get_child_end()){
                existing_conn = test_conn;
                break;
            }
        }

        int return_status;
        if (existing_conn){
            // If the connection exists for this branch, update existing connection
            existing_conn->increment_confidence(connection->get_confidence());
            existing_conn->add_reconstructions(connection->get_reconstructions());
            // Connection already exists in terms of parent and child nodes and ends, make sure the parent does not contain it
            if (connection->get_parent())
                connection->get_parent()->remove_child_connection(connection); // !Shouldn't be necessary!
            connection = existing_conn;
            return_status = 1;
        }else{
            // Otherwise insert the new connection
            parent_connections.insert(connection);
            return_status = 2;
        }

        if (connection->get_parent()){
            connection->get_parent()->add_child_connection(connection);
        }

        if (!best_connection || connection->get_confidence() > best_connection->get_confidence()){
            best_connection = connection;
        }
        return return_status;
    }else{
        printf("ERROR! the child != this\n");
        return 0;
    }
};
/*
void CompositeBranchContainer::import_connection(Connection *connection){
    if (this == connection->get_child())
        parent_connections.insert(connection);
};*/

// Might need to be tested and revisited, but I think this will work
void CompositeBranchContainer::remove_connection(Connection *connection){
    //if (this == connection->get_child()){
    //ConnectionPtrSet::iterator con_pos = parent_connections.find(connection);
    //if (con_pos != parent_connections.end()){
    if (parent_connections.find(connection) != parent_connections.end()){
        combined_connection_weight -= connection->get_confidence();

        parent_connections.erase(connection);
        //parent_connections.erase(con_pos);
        if (best_connection == connection){
            // Determine next best connection
            double highest_conf = 0;
            for (Connection *candidate : parent_connections){
                if (candidate->get_confidence() > highest_conf){
                    highest_conf = candidate->get_confidence();
                    best_connection = candidate;
                }
            }
        }
    }else{
        printf("ERROR! connection with this branch as child isn't in branch's parent_connections, does the connection child match? %i\n", this==connection->get_child());
    }
};

ConnectionPtrSet CompositeBranchContainer::get_connections() const{
    return parent_connections;
};
ConnectionPtrSet CompositeBranchContainer::get_child_connections(BranchEnd branch_end) const{
    if (branch_end == BOTTOM)
        return bottom_child_connections;
    else
        return top_child_connections;
};
double CompositeBranchContainer::get_combined_connection_weight() const{
    return combined_connection_weight;
}

void CompositeBranchContainer::add_child_connection(Connection *connection){
    if (connection->get_parent_end() == TOP){
        top_child_connections.insert(connection);
    }else{
        bottom_child_connections.insert(connection);
    }
};

void CompositeBranchContainer::remove_child_connection(Connection *connection){
    if (connection->get_parent_end() == TOP){
        top_child_connections.erase(connection);
    }
    else{
        bottom_child_connections.erase(connection);
    }
}
// No longer used, likely to cause some problems with memory management of connections
void CompositeBranchContainer::remove_child_connections(BranchEnd branch_end){
    ConnectionPtrSet *child_connections;
    if (branch_end == TOP){
        for (Connection *connection : top_child_connections){
            connection->get_child()->remove_connection(connection);
        }
        top_child_connections.erase(top_child_connections.begin(), top_child_connections.end());
    }else{
        //child_connections = &bottom_child_connections;
        for (Connection *connection : bottom_child_connections){
            connection->get_child()->remove_connection(connection);
        }
        bottom_child_connections.erase(bottom_child_connections.begin(), bottom_child_connections.end());

    }
};


// Returns true if end1 is the top, false if end2 is the top (end1 is tail)
// double gives certainty of the decision (0 means 50/50, 1 means definitely the given end is top)
pair<bool,double> CompositeBranchContainer::get_direction_certainty(){
    float dir1_conf = 0, dir2_conf = 0;
    for(Connection *connection : parent_connections) {
        if (connection->get_child_end() == TOP){
            dir1_conf += connection->get_confidence();
        }else{
            dir2_conf += connection->get_confidence();
        }
    }
    /*
     typedef std::map<CompositeBranchContainer, float>::iterator it_type;
    for(it_type iterator = end_parents1.begin(); iterator != m.end(); iterator++) {
        dir1_conf += iterator->second;
    }
    for(it_type iterator = end_parents2.begin(); iterator != m.end(); iterator++) {
        dir2_conf += iterator->second;
    }
     */
     
    pair<bool,double> return_val;
    return_val.first = dir1_conf > dir2_conf;
    return_val.second = abs(dir1_conf-dir2_conf)/(dir1_conf+dir2_conf);
    return return_val;
};

// Returns pair of most likely connection and the entropy of the possibilities
pair<Connection*,double> CompositeBranchContainer::get_connection_entropy(){
    double entropy = 0, total_confidence = 0;
    typedef pair<Connection*, double> conn_entropy_pair;

    Connection *most_probable = nullptr;
    double highest_conf = 0;
    for(Connection *connection : parent_connections) {
        total_confidence += connection->get_confidence();
        if (most_probable == nullptr || connection->get_confidence() > most_probable->get_confidence()){
            most_probable = connection;
        }
    }
    for(Connection * connection : parent_connections) {
        double connection_prob = connection->get_confidence() / total_confidence;
        entropy += connection_prob * log(connection_prob);
    }
    
    return conn_entropy_pair(most_probable,entropy);
};

// Returns pair of most likely connection and the entropy of the possibilities given one end of the branch
pair<Connection *,double> CompositeBranchContainer::get_connection_entropy(BranchEnd branch_end){
    double entropy = 0, total_confidence = 0;
    typedef pair<Connection*, double> conn_entropy_pair;
    
    Connection *most_probable = nullptr;
    double highest_conf = 0;
    for(Connection *connection : parent_connections) {
        if (branch_end == connection->get_child_end()){
            total_confidence += connection->get_confidence();
            if (most_probable == nullptr || connection->get_confidence() > most_probable->get_confidence()){
                most_probable = connection;
            }
        }
    }
    for(Connection *connection : parent_connections) {
        if (branch_end == connection->get_child_end()){
            double connection_prob = connection->get_confidence() / total_confidence;
            entropy += connection_prob * log(connection_prob);
        }
    }
    
    return conn_entropy_pair(most_probable,entropy);
};

pair<Connection *,double> CompositeBranchContainer::get_best_connection_probability() const{
    if (best_connection){
//        printf("best_connection %p, combined_conn_weight %f\n",best_connection,combined_connection_weight);
        return pair<Connection*,double>(best_connection, best_connection->get_confidence() / combined_connection_weight);
    }
    return pair<Connection*,double>(nullptr,0);
};
void CompositeBranchContainer::calculate_best_connection(){
    best_connection = nullptr;
    for (Connection *connection : parent_connections){
        if (!best_connection || connection->get_confidence() > best_connection->get_confidence()){
            best_connection = connection;
        }
    }
};
Connection * CompositeBranchContainer::get_best_connection() const{
    return best_connection;
};
pair<Connection *,double> CompositeBranchContainer::get_best_connection_weight() const{
//    printf("best_connection %p\n",best_connection);
    if (best_connection){
  //      printf("combined_conn_weight %f\n",best_connection,combined_connection_weight);
        return pair<Connection *,double>(best_connection, best_connection->get_confidence());
    }
    return pair<Connection *,double>(nullptr,0);
};

// Better to use a different measure than best_connection_probability? Maybe entropy or some other alternative?
bool compare_branch_pointers_by_confidence(const CompositeBranchContainer *first, const CompositeBranchContainer *second){
    return first->get_best_connection_probability().second > second->get_best_connection_probability().second;
};
bool compare_branch_pointers_by_weight(const CompositeBranchContainer *first, const CompositeBranchContainer *second){
    return first->get_best_connection_weight().second > second->get_best_connection_weight().second;
};
bool compare_branches_by_confidence(const CompositeBranchContainer &first, const CompositeBranchContainer &second){
    return first.get_best_connection_probability().second > second.get_best_connection_probability().second;
};

bool is_loop(CompositeBranchContainer *node){
    std::set<CompositeBranchContainer *> traversed_nodes;
    while (true){
        if (!node) return false;
        traversed_nodes.insert(node);
        node = node->get_parent();
        if (traversed_nodes.find(node) != traversed_nodes.end()){
            return true;
        }
    }
};
bool creates_loop(CompositeBranchContainer *parent, CompositeBranchContainer *child){
    std::set<CompositeBranchContainer *> traversed_nodes;
    while (true){
        if (!parent) return false;
        if (parent == child) return true;
        traversed_nodes.insert(parent);
        parent = parent->get_parent();
        if (traversed_nodes.find(parent) != traversed_nodes.end()){
            printf("ERROR: Parent already in a loop apart from candidate child (thrown from Composite.cpp:creates_loop\n");
            throw "Parent already in a loop apart from candidate child";
        }
    }
};

/*******************
 **  Connection   **
 *******************/

unsigned long Connection::master_uid = 0;

Connection::Connection(CompositeBranchContainer *child, BranchEnd child_end, CompositeBranchContainer *parent, BranchEnd parent_end, Reconstruction *reconstruction, double confidence){
    uid = Connection::master_uid++;
    c_child = child;
    c_child_end = child_end;
    c_parent = parent;
    c_parent_end = parent_end;
    c_reconstructions.insert(reconstruction);
    if (confidence == 0){
        confidence = reconstruction->get_confidence();
    }
    c_confidence = confidence;
    /*
    if (!parent){
        printf("parent is nullptr for connection (1) \n");
    }
     */
};
Connection::Connection(CompositeBranchContainer *child, BranchEnd child_end, CompositeBranchContainer *parent, BranchEnd parent_end, std::set<Reconstruction *> reconstructions, double confidence){
    uid = Connection::master_uid++;
    c_child = child;
    c_child_end = child_end;
    c_parent = parent;
    c_parent_end = parent_end;
    c_reconstructions.insert(reconstructions.begin(), reconstructions.end());
    c_confidence = confidence;
    /*
    if (!parent){
        printf("parent is nullptr for connection (2) \n");
    }
     */
};
Connection::Connection(Connection *connection){
    uid = Connection::master_uid++;
    c_child = connection->get_child();
    c_child_end = connection->get_child_end();
    c_parent = connection->get_parent();
    c_parent_end = connection->get_parent_end();
    add_reconstructions(connection->get_reconstructions());
    c_confidence = connection->get_confidence();
    /*
    if (!connection->get_parent()){
        printf("parent is nullptr for connection (3) \n");
    }
     */
}
void Connection::set_confidence(double confidence){
    c_confidence = confidence;
};
void Connection::increment_confidence(double confidence){
    c_confidence += confidence;
};
void Connection::add_reconstruction(Reconstruction *reconstruction){
    c_reconstructions.insert(reconstruction);
};
void Connection::add_reconstructions(std::set<Reconstruction *> reconstructions){
    c_reconstructions.insert(reconstructions.begin(), reconstructions.end());
};

CompositeBranchContainer * Connection::get_parent() const{
    return c_parent;
};
CompositeBranchContainer * Connection::get_child() const{
    return c_child;
};
BranchEnd Connection::get_parent_end() const{
    return c_parent_end;
};
void Connection::set_parent_end(BranchEnd parent_end){
    c_parent_end = parent_end;
}
void Connection::set_parent(CompositeBranchContainer *parent){
    c_parent = parent;
    /*
    if (!parent){
        printf("parent is nullptr for connection (4) \n");
    }
     */
};
BranchEnd Connection::get_child_end() const{
    return c_child_end;
};
void Connection::set_child_end(BranchEnd child_end){
    c_child_end = child_end;
}
void Connection::set_child(CompositeBranchContainer *child){
    c_child = child;
};
double Connection::get_confidence() const{
    return c_confidence;
};
std::set<Reconstruction *> Connection::get_reconstructions(){
    return c_reconstructions;
};

bool compare_connections(Connection &first, Connection &second){
    return first.get_confidence() > second.get_confidence();
};

bool compare_connection_ptrs(Connection *first, Connection *second){
    return first->get_confidence() > second->get_confidence();
};

bool connection_ptr_less::operator()(const Connection *lhs, const Connection *rhs) const{
    return !lhs ? rhs ? true : false : !rhs ? false :
    lhs->get_uid() < rhs->get_uid();
};

/*******************
 **   Composite   **
 *******************/

Composite::Composite(Logger *logger){
    if (logger){
        this->logger = logger;
        delete_logger = false;
    }else{
        this->logger = new Logger();
        delete_logger = true;
    }
};

// For initializing composite to the first reconstruction
Composite::Composite(Reconstruction *reconstruction, Logger *logger){
    if (logger){
        this->logger = logger;
        delete_logger = false;
    }else{
        this->logger = new Logger();
        delete_logger = true;
    }

    // Add reconstruction to vector
    reconstructions.push_back(reconstruction);
    //composite_reconstruction = reconstruction;
    //c_root_segment = copy_segment_tree(reconstruction->get_tree());
    add_first_reconstruction(reconstruction);
};

Composite::~Composite(){
    if (logger && delete_logger) delete logger;
}

void Composite::delete_all(){
    logger->debug3("Entering Composite::delete_all");
    std::set<CompositeBranchContainer *> branch_set = get_branches();
    for (CompositeBranchContainer *c_branch : branch_set){
        c_branch->prepare_for_deletion();
    }
    
    for (CompositeBranchContainer *c_branch : branch_set){
        for (MyMarker *marker : c_branch->get_segment()->markers){
            delete marker;
        }
        delete c_branch->get_segment();

        c_branch->delete_connections();
        delete c_branch;
    }
}

void Composite::set_root(CompositeBranchContainer *root){
    c_root = root;
    c_roots.clear();
    c_root_segments.clear();
    c_roots.push_back(root);
    if (root){
        c_root_segment = root->get_segment();
        c_root_segments.push_back(c_root_segment);
    }else{
        c_root_segment = nullptr;
        c_root_segments.push_back(nullptr);
    }
};

void Composite::add_root(CompositeBranchContainer *composite_branch){
    if (composite_branch){
        if (!c_root){
            c_root = composite_branch;
            c_root_segment = composite_branch->get_segment();
        }
        logger->debug4("add_root %p, branch children %i, segment children %i",composite_branch,composite_branch->get_children().size(),composite_branch->get_segment()->child_list.size());
        c_roots.push_back(composite_branch);
        c_root_segments.push_back(composite_branch->get_segment());
    }
};
void Composite::clear_roots(){
    c_roots.clear();
    c_root_segments.clear();
};


void Composite::add_first_reconstruction(Reconstruction *reconstruction){
    // Traverse reconstruction: copy branches and create decision points
    logger->debug1("Entered add_first_reconstruction");
    NeuronSegment *segment, *composite_segment;
    BranchContainer *branch, *branch_parent;
    CompositeBranchContainer *composite_parent;
    
    std::set<pair<CompositeBranchContainer *, CompositeBranchContainer *> > connection_pair;
    Connection *connection;

    // Determine whether segment markers go first to last or last to first
    bool markers_forward = false;
    for (NeuronSegment *segment : reconstruction->get_segments()){
        if (segment->markers.size() > 1){
            markers_forward = segment->markers[0]->parent != segment->markers[1];
            break;
        }
    }

    // Push root of reconstruction, and null pointer as there is no parent
    std::stack<pair<NeuronSegment *, BranchContainer *> > segment_stack;
    for (NeuronSegment *segment : reconstruction->get_trees()){
        segment_stack.push(pair<NeuronSegment *, BranchContainer *>(segment,nullptr));
    }
    //segment_stack.push(pair<NeuronSegment *, BranchContainer *>(reconstruction->get_tree(),nullptr));
    pair<NeuronSegment *, BranchContainer *> segment_pair;

    // Process each branch (create CompositeBranchContainers and DecisionPoints), handling via a stack
    int count = 0;
    c_root = nullptr;
    c_root_segment = nullptr;
    c_roots.clear();
    c_root_segments.clear();
    while (!segment_stack.empty()){
        count++;
        segment_pair = segment_stack.top();
        segment_stack.pop();
        segment = segment_pair.first;

        // Get parent of current reconstruction branch
        branch_parent = segment_pair.second;

        // Create branch container for this segment
        //branch = new BranchContainer(reconstruction,segment,branch_parent);
        branch = reconstruction->get_branch_by_segment(segment);

        // Copy segment and create composite branch out of it
        composite_segment = copy_segment_markers(segment);
        
        CompositeBranchContainer *composite_branch = new CompositeBranchContainer(composite_segment, this);
        
        // Create links between branch and composite branch
        composite_branch->add_branch_match(branch, true);
        branch->set_composite_match(composite_branch);

        // Create a Connection if the branch has a parent
        if (!branch_parent){
            logger->debug4("No branch parent");
            // If this is the root, create new composite reconstruction pointing to the root composite segment
            add_root(composite_branch);
            composite_branch->add_connection(TOP, nullptr, BOTTOM, reconstruction, reconstruction->get_confidence());
        }else{
            // Get composite parent of new composite branch
            logger->debug4("Yes branch parent");
            composite_parent = branch_parent->get_composite_match();
            logger->debug4("Setting branch parent");
            composite_branch->set_parent(composite_parent);
            
            // Add Connection
            logger->debug4("Adding connection");
            logger->debug4("Confidence: %f",reconstruction->get_confidence());
            composite_branch->add_connection(TOP, composite_parent, BOTTOM, reconstruction, reconstruction->get_confidence());

            NeuronSegment *parent_seg = composite_parent->get_segment();
            // Link marker at top of segment to it's parent marker
            if (markers_forward){
                // The first marker is the beginning of the segment
                segment->markers[0]->parent = parent_seg->markers.back();
            }else{
                // The last marker is the beginning of the segment
                segment->markers.back()->parent = parent_seg->markers[0];
            }
        }
        
        // Add children of the current segment along with the newly created composite segment for connecting
        for (NeuronSegment *child : segment->child_list){
            segment_stack.push(pair<NeuronSegment *,BranchContainer *>(child, branch));
        }
    }
    /*
    // Update marker head parent connection
    logger->debug4("add_first_reconstruction: Test update marker parents");
    for (CompositeBranchContainer * branch : get_branches()){
        if (branch->get_parent()){
        }
    }
    logger->debug4("add_first_reconstruction: End test update marker parents");
*/
    
    logger->debug("Processed %d segments in creating first composite",count);
};

/* #TODO: Takes in new branch and ???
void Composite::process_branch(BranchContainer * branch){
    
};
*/
//Composite::logger = new Logger(0);
//Logger * Composite::logger = new Logger(0);
void Composite::set_logger(Logger *logger) {
    if (logger){
        if (delete_logger) delete this->logger;
        this->logger = logger;
        delete_logger = false;
    }
};

CompositeBranchContainer * Composite::get_root(){
    return c_root;
};
std::vector<CompositeBranchContainer *> Composite::get_roots(){
    std::vector<CompositeBranchContainer *> roots_copy = c_roots;
    return roots_copy;
};
/*
Reconstruction * Composite::get_composite_reconstruction(){
    return composite_reconstruction;
}*/
NeuronSegment * Composite::get_root_segment(){
    return c_root_segment;
};
std::vector<NeuronSegment *> Composite::get_root_segments(){
    return c_root_segments;
};
SegmentPtrSet Composite::get_segments(){
    return c_segments;
}
std::vector<NeuronSegment *> Composite::get_segments_ordered(){
    std::vector<NeuronSegment *> ordered_segs = produce_segment_vector(c_root_segment);
    /* get_segments_ordered is specifically used to get the segments that are part of a single tree for tree matching
    // produce_segment_vector only gets segments that are connected to the root segment
    std::set<NeuronSegment *> added_segs(ordered_segs.begin(),ordered_segs.end());
    // Go through all segments and add any that were missed
    for (NeuronSegment *seg : c_segments){
        if (added_segs.find(seg) == added_segs.end()){
            ordered_segs.push_back(seg);
        }
        // Could alternatively trace back up to put these other segments in order, but for now I don't see a pressing reason to do so
    }
     */
    return ordered_segs;
};

void Composite::add_reconstruction(Reconstruction *reconstruction){
    if (reconstructions.size() == 0){
        //composite_reconstruction = &reconstruction;
        add_first_reconstruction(reconstruction);
    }
    reconstructions.push_back(reconstruction);
    summary_confidence += reconstruction->get_confidence();
};

void Composite::add_branch(CompositeBranchContainer *branch){
    c_branches.insert(branch);
//    logger->debug3("add_branch with segment pointer %p, branch pointer %p",branch->get_segment(), branch);
    c_segments.insert(branch->get_segment());
    segment_container_map[branch->get_segment()] = branch;
};
void Composite::remove_branch(CompositeBranchContainer *branch){
    c_branches.erase(branch);
    segment_container_map.erase(branch->get_segment());
    c_segments.erase(branch->get_segment());
};

std::vector<Reconstruction*> Composite::get_reconstructions(){
    return reconstructions;
};

double Composite::get_summary_confidence(){
    return summary_confidence;
};

std::set<CompositeBranchContainer*> Composite::get_branches(){
    return c_branches;
    /*
    std::set<CompositeBranchContainer*> compiled_branches;
    std::stack<CompositeBranchContainer*> cb_stack;
    cb_stack.push(c_root);
    CompositeBranchContainer * branch;
    while (!cb_stack.empty()){
        branch = cb_stack.top();
        cb_stack.pop();
        compiled_branches.insert(branch);
        for (CompositeBranchContainer * child : branch->get_children()){
            cb_stack.push(child);
        }
    }
    return compiled_branches;
     */
};
CompositeBranchContainer * Composite::get_branch_by_segment(NeuronSegment * segment){
    return segment_container_map[segment];
};
typedef map<NeuronSegment *,CompositeBranchContainer *> SegmentCBContainerMap;
typedef pair<NeuronSegment *,CompositeBranchContainer *> SegmentCBContainerPair;
void Composite::update_branch_by_segment(NeuronSegment * segment, CompositeBranchContainer * branch){
    segment_container_map[segment] = branch;
};

Composite * Composite::copy(){
    Composite *copy = new Composite();
    
    logger->new_line();
    logger->debug("Composite::copy()");
    std::map<CompositeBranchContainer *,CompositeBranchContainer *> new_to_old_map;
    std::map<CompositeBranchContainer *,CompositeBranchContainer *> old_to_new_map;
    
    // Code block below is only for debugging
    logger->debug1("Composite::copy  creating new branches");
    std::set<CompositeBranchContainer*> tmp = c_root->get_children();
    logger->debug1("Composite::copy another earlier attempt at get_children on c_root - has %i children",tmp.size());
    std::vector<NeuronSegment*> tmp2 = c_root_segment->child_list;
    logger->debug1("Composite::copy another earlier attempt at get_children on c_root_segment - has %i children",tmp2.size());
    for (CompositeBranchContainer* child : tmp){
        logger->debug4("before ->get_segment, child %p", child);
        if (child){
            NeuronSegment *sg = child->get_segment();
            logger->debug4("got child segment %p",sg);
            logger->debug4("%i markers, first: %f %f %f",sg->markers.size(),sg->markers[0]->x,sg->markers[0]->y,sg->markers[0]->z);
            for (int i = 0; i < sg->markers.size(); i++){
                MyMarker *marker = sg->markers[i];
            }
        }
    }
    // End debug block
    
    // Copy all branches (including their segments and markers, and connections); map orig to copy and copy to orig
    logger->debug3("number of branches in this composite %i",c_branches.size());
    for (CompositeBranchContainer *branch : c_branches){
        logger->debug4("Branch ptr %p, segment ptr %p",branch,branch->get_segment());
        CompositeBranchContainer *branch_copy = new CompositeBranchContainer(branch, copy);
        new_to_old_map[branch_copy] = branch;
        old_to_new_map[branch] = branch_copy;
    }

    // Create parent/child pointers (including in connections)
    logger->debug1("Composite::copy connecting branches");
    int count = 0;
    for (std::map<CompositeBranchContainer *,CompositeBranchContainer *>::iterator it = new_to_old_map.begin(); it != new_to_old_map.end(); ++it){
        CompositeBranchContainer *new_branch = it->first;
        CompositeBranchContainer *orig_branch = it->second;
        CompositeBranchContainer *orig_parent = orig_branch->get_parent();
        
        logger->debug4("new_branch %p, Num connections %i",new_branch,new_branch->get_connections().size());
        // Connections have been copied, now their references must be updated!
        for (Connection *connection : new_branch->get_connections()){
            connection->set_child(new_branch);
            if (connection->get_parent()){
                CompositeBranchContainer *new_parent = old_to_new_map[connection->get_parent()];
                connection->set_parent(new_parent);
                new_parent->add_child_connection(connection);
            }
        }

        count++;
        //printf("Branch connecting %d; orig_parent %p\n",count,orig_parent);
        
        if (orig_parent){
            CompositeBranchContainer *new_parent = old_to_new_map[orig_parent];
            // Make child->parent connection
            new_branch->set_parent(new_parent);
        }else {
            // No parent
            logger->debug4("no parent");
        }
    }
    
    copy->set_root(old_to_new_map[c_root]);
    for (CompositeBranchContainer *root : c_roots){
        if (root != c_root) // Need to avoid adding primary root a second time
            copy->add_root(old_to_new_map[root]);
    }
    logger->debug("Done making copy");
    
    return copy;
};

void Composite::update_tree(){
    logger->debug("Start Composite::update_tree()");
    //Composite * running_consensus = generate_consensus(0, false);
    convert_to_consensus(0,0,true);
    
    logger->debug2("End of update_tree");
}

Composite * Composite::generate_consensus(int inclusion_vote_threshold, int rescue_vote_threshold, bool multiple_trees){
    double inclusion_confidence_threshold = (double)inclusion_vote_threshold / reconstructions.size();
    double rescue_confidence_threshold = (double)rescue_vote_threshold / reconstructions.size();
    return generate_consensus(inclusion_confidence_threshold, rescue_confidence_threshold, multiple_trees);
}
Composite * Composite::generate_consensus(double branch_inclusion_threshold, double branch_rescue_threshold, bool multiple_trees){
    // Copy CompositeBranch to allow for removal of branches and connections below threshold

    logger->debug4("Before composite copy; check root_branch %p %i",c_root,c_root->get_children().size());
    logger->debug4("Before composite copy; check root_segment %p %i",c_root_segment,c_root_segment->child_list.size());

    logger->debug("Making composite copy");
    Composite *consensus = copy(); // Segment pointers transfered, not copied
    consensus->convert_to_consensus(branch_inclusion_threshold, branch_rescue_threshold, multiple_trees);

    // Return consensus reconstruction object containing NeuronSegment* root
    return consensus;
};

std::map<NeuronSegment *, double> Composite::get_segment_confidences(){
    std::map<NeuronSegment *, double> confidence_map;
    logger->debug4("Composite::get_segment_confidences");
    for (NeuronSegment *segment : c_segments){
        CompositeBranchContainer *branch = segment_container_map[segment];
        confidence_map[segment] = branch->get_confidence();
    }
    return confidence_map;
};

std::map<NeuronSegment *, int> Composite::get_segment_confidence_counts(){
    std::map<NeuronSegment *, int> confidence_map;
    logger->debug4("Composite::get_segment_confidence_counts");
    for (NeuronSegment *segment : c_segments){
        CompositeBranchContainer * branch = segment_container_map[segment];
        confidence_map[segment] = branch->get_branch_matches().size();
    }
    return confidence_map;
};

std::map<NeuronSegment *, double> Composite::get_connection_confidences(){
    std::map<NeuronSegment *, double> confidence_map;
    logger->debug4("Composite::get_connection_confidences");
    for (NeuronSegment *segment : c_segments){
        CompositeBranchContainer *branch = segment_container_map[segment];
        confidence_map[segment] = branch->get_best_connection_probability().second;
    }
    return confidence_map;
};

std::map<NeuronSegment *, int> Composite::get_connection_confidence_counts(){
    std::map<NeuronSegment *, int> confidence_map;
    logger->debug4("Composite::get_segment_confidences");
    for (NeuronSegment *segment : c_segments){
        CompositeBranchContainer * branch = segment_container_map[segment];
        confidence_map[segment] = branch->get_best_connection_probability().first->get_reconstructions().size();
    }
    return confidence_map;
};

void Composite::convert_to_consensus(double branch_inclusion_threshold, double branch_rescue_threshold, bool multiple_trees){
    // Create set of segments that are below the confidence threshold
    // (they can be added back in in order to attach a branch/subtree that is otherwise unconnected from the rest of the tree)
    logger->info("Entering Composite::convert_to_consensus with threshold %f and %f, multiple_trees %i",branch_inclusion_threshold, branch_rescue_threshold, multiple_trees);
    
    if (branch_inclusion_threshold >= 1){
        branch_inclusion_threshold /= reconstructions.size();
    }
    if (branch_rescue_threshold >= 1){
        branch_rescue_threshold /= reconstructions.size();
    }
    
    // Remove all parent/child relationships of all branches
    logger->debug("Clearing parent/child relationships");

    std::set<CompositeBranchContainer *> copied_branches_set = c_branches;

    logger->debug2("generate consensus 1, branch_inclusion_threshold %f",branch_inclusion_threshold);
    // Mark branches with confidence below threshold so that they are only brought into consensus if required for connecting above threshold branches to the consensus tree
    std::set<CompositeBranchContainer *> removed_segments;
    for (CompositeBranchContainer *branch : copied_branches_set){
        //printf("branch %p\n",branch);
        // Checking whether has high confidence
        logger->debug3("Branch starting at %f %f %f has confidence %f, being removed?",branch->get_segment()->markers[0]->x,branch->get_segment()->markers[0]->y,branch->get_segment()->markers[0]->z,branch->get_confidence());
        logger->debug3("Summed conf %f, conf denominator %f",branch->get_summed_confidence(),branch->get_confidence_denominator());
        if ((double)branch->get_confidence() < (double)branch_inclusion_threshold){
            logger->debug4("Branch is below threshold and so will be removed %f < %f",branch->get_confidence(),branch_inclusion_threshold);
            removed_segments.insert(branch);
        }
        
        // Removing connections
        branch->set_parent(nullptr);
        branch->remove_children();
    }
    logger->debug4("generate consensus 2, num removed segments %i",removed_segments.size());

    // Remove segments from copied_branches
    for (CompositeBranchContainer *branch : removed_segments){
        copied_branches_set.erase(branch);
    }
    // Copy branches from set to vector for sorting
    std::vector<CompositeBranchContainer *> copied_branches;
    for (CompositeBranchContainer *branch : copied_branches_set){
        copied_branches.push_back(branch);
    }
    std::set<CompositeBranchContainer *> processed_branches;
    
    // #TODO: Loop while any connection_sets remain, taking the most confident (least conflicting) connection_set each loop
    //  Currently just going through with the initial confidence order
    
    /* Make decision on parentage and connectivity, setting directionality on each branch that gets connected (those that are not connected at all  must have a parent via the other decision points) */
    
    // Sort connection_sets by minimization of conflict; Start with branch with highest weight (combined confidence votes)
    std::sort(copied_branches.begin(), copied_branches.end(), &compare_branch_pointers_by_weight);
    logger->debug4("generate consensus 2.5; num copied branches %i",copied_branches.size());
    
    pair<NeuronSegment *,double> root_seg_conf = pair<NeuronSegment *,double>(nullptr,0);

    c_root = nullptr;
    c_root_segment = nullptr;
    c_roots.clear();
    c_root_segments.clear();

    // Connections that cannot be chosen because they topologically conflict with previously chosen connections
    ConnectionPtrSet eliminated_connections;
    ConnectionPtrSet connect_to_top;
    
    std::map<CompositeBranchContainer *,BranchEnd> connected_parent_end_map;
    std::map<pair<CompositeBranchContainer *,BranchEnd>, std::set<CompositeBranchContainer *> > connected_child_end_map;
    int numMarkers = 0;
    
    CompositeBranchContainer *branch = nullptr;
    while (branch || !copied_branches.empty()){
        if (!branch){
            // Update order since modifying connections
            std::sort(copied_branches.begin(), copied_branches.end(), &compare_branch_pointers_by_weight);
            branch = *(copied_branches.begin());
        }
        processed_branches.insert(branch); // Add branch to set of processed branches - so it is never run again
        // Remove current branch from vector of branches
        // #TODO: Replace this with a sorted set, or something that would be faster
//        logger->info("Remaining branches before %i",copied_branches.size());
        copied_branches.erase(std::remove(copied_branches.begin(),copied_branches.end(),branch),copied_branches.end());
//
        numMarkers += branch->get_segment()->markers.size();

        logger->debug1("Branch %p with confidence %f and best connection total conf weight %f",branch, branch->get_confidence(), branch->get_best_connection()->get_confidence());
        

        // Get branch's connections and sort by confidence
        ConnectionPtrSet connections_set = branch->get_connections();
        logger->debug2("Number of available connections %i",connections_set.size());
        logger->debug2("Segment %p, num_markers %i",branch->get_segment(),branch->get_segment()->markers.size());
        logger->debug2("branch votes %f, conf_denom %f, best connection %p", branch->get_summed_confidence(), branch->get_confidence_denominator(), branch->get_best_connection());
//        logger->debug2("connection votes %f", branch->get_best_connection()->get_confidence());
        logger->debug2("Segment first marker %f %f %f",branch->get_segment()->markers[0]->x,branch->get_segment()->markers[0]->y,branch->get_segment()->markers[0]->z);
        std::vector<Connection *> connections(connections_set.begin(),connections_set.end());
        if (connections.size() == 0){
            if (removed_segments.find(branch) != removed_segments.end())
                logger->warn("Probable error, this branch is above threshold but has no remaining potential connections");
            branch = nullptr;
            continue;
        }
        std::sort(connections.begin(), connections.end(), &compare_connection_ptrs);
        
        // ** Resolve connection_set: 1. Determine best connection/parent **
        
        // In order of the connection with the greatest confidence, choose the first connection to a branch that has sufficient confidence
        Connection *chosen_connection = nullptr;
        CompositeBranchContainer *chosen_parent;

//        for (std::vector<Connection *>::iterator conn_it = connections.begin(); conn_it != connections.end() && !chosen_connection; ++conn_it){
        bool blocked_branches = false;
        for (Connection *connection : connections){
            //if (eliminated_connections.find(connection) != eliminated_connections.end()) continue;
            
            logger->debug2("processing connection %p with parent %p and confidence %f",connection,connection->get_parent(), connection->get_confidence());
            if (connection->get_parent())
                logger->debug4("parent segment %p",connection->get_parent()->get_segment());
            // Ensure branch has sufficient confidence not to have been removed
            if (removed_segments.find(connection->get_parent()) == removed_segments.end()){
                // Ensure this connection would not create a loop
                if (!creates_loop(connection->get_parent(),branch)){
                    logger->debug2("connection chosen %p",connection);
                    chosen_connection = connection;
                    break;
                }else{
                    logger->debug3("Found connection that would cause a loop");
                    if (connection->get_child() != branch){
                        logger->error("branch is not connection's child!");
                    }
                }
            }else{
                blocked_branches = true;
                logger->debug1("Branch blocked");
            }
        }
        
        // If require single tree and none of the parents are above threshold confidence, try out connections to branches below threshold (if any)
        if (!chosen_connection){
            logger->debug1("No chosen connection");
            /*
            if (multiple_trees){
                logger->debug1("multriple_trees allowed, so make this branch a root");
                chosen_parent = nullptr;
            }else{
             */
            if (!blocked_branches){
                //
                if (multiple_trees){
                    logger->debug1("multriple_trees allowed, so make this branch a root");
                    chosen_parent = nullptr;
                }else{
                    logger->debug1("No connection found, and no branches were blocked, must only be loops. Leave this branch out");
                    branch = nullptr;
                    continue;
                }
            }else{
                
                logger->debug2("No connection found, see if one can be rescued of %i connections",connections.size());
                
                // One of the parents must be chosen using a combination of their branch confidence and the connection confidence
                double best_confidence = 0;
                for (Connection *connection : connections){
                    //if (eliminated_connections.find(connection) != eliminated_connections.end()) continue;
                    
                    logger->debug4("connection %p",connection);
                    if (connection->get_parent() && connection->get_parent()->get_confidence() >= branch_rescue_threshold){
                        double joint_conf = connection->get_confidence() * connection->get_parent()->get_confidence();
                        if (joint_conf > best_confidence && !creates_loop(connection->get_parent(),branch)){
                            best_confidence = joint_conf;
                            chosen_connection = connection;
                        }
                    }
                }
                // If we have found a connection via a removed branch, add the branch to the end of copied_branches
                if (chosen_connection){
                    logger->debug1("Connection via sub-threshold branch found %i %i %i, sub-threshold branch being queued up for inclusion",
                                   branch->get_segment()->markers[0]->x,branch->get_segment()->markers[0]->y,branch->get_segment()->markers[0]->z);
                    if (chosen_parent){
                        copied_branches.push_back(chosen_connection->get_parent());
                        // Erase the parent branch from the set of removed segments (this will ensure it doesn't get queued up multiple times)
                        removed_segments.erase(chosen_connection->get_parent());
                    }
                    chosen_parent = chosen_connection->get_parent();
                }else if (multiple_trees){
                    logger->debug1("multriple_trees allowed, no rescuable branches, so make this branch a root");
                    chosen_parent = nullptr;
                }else{
                    logger->debug1("No non-loop forming connection possible for this branch given the branch_rescue_threshold.");
                    branch = nullptr;
                    continue;
                }
            }
        }else{
            logger->debug2("Chosen connection found");
            chosen_parent = chosen_connection->get_parent();
        }

        // If no parent, determine whether this branch is a root or whether another connection should be used
        if (!chosen_parent){
            // Check whether this branch is a/the root branch
            logger->debug1("No parent, is this the root branch? %i",!c_root);
            if (multiple_trees || !c_root){
                logger->debug1("Multiple trees allowed, adding this branch as a root");
                add_root(branch);
            }else{
                // Choose the next best connection
                logger->debug1("There is already a root, so choose the next best connection");
                //for (std::vector<Connection *>::iterator conn_it = connections.begin(); conn_it != connections.end(); ++conn_it){
                for (Connection *connection : connections){
                    //if (eliminated_connections.find(connection) != eliminated_connections.end()) continue;
                    
                    if (connection != chosen_connection && !creates_loop(connection->get_parent(),branch) && (!connection->get_parent() || removed_segments.find(connection->get_parent()) == removed_segments.end())){
                        chosen_connection = connection;
                        chosen_parent = chosen_connection->get_parent();
                        break;
                    }
                }
                // If no parent is found? Currently do nothing
                if (chosen_parent){
                    logger->debug1("Found alternative parent");
                }else{
                    logger->debug1("No alternative parent found! This branch will not be connected");
                    // Move on from this branch, leaving it unconnected
                    branch = nullptr;
                    continue;
                }
            }
        }
        
        logger->debug4("branch %p, parent %p",branch,chosen_parent);
        logger->debug4("branch markers %i",branch->get_segment()->markers.size());

        // ** Create connections in consensus **

        // Now set parent
        BranchEnd parent_end, child_end = TOP;
        if (chosen_connection) {
            parent_end = chosen_connection->get_parent_end();
            child_end = chosen_connection->get_child_end();
        }

        logger->debug2("chosen_parent in connected_parent_end_map %i",connected_parent_end_map.find(chosen_parent) != connected_parent_end_map.end());
        if (connected_parent_end_map.find(chosen_parent) != connected_parent_end_map.end()){
            logger->debug4("value %i, parent_end %i",connected_parent_end_map[chosen_parent],parent_end);
        }
        // If the parent has already taken a direction and this will connect the child to the parent's (current) TOP
        if (connected_parent_end_map.find(chosen_parent) != connected_parent_end_map.end() && connected_parent_end_map[chosen_parent] == parent_end){
            // Split the parent to enable connection - creates a branch of length 1 of which the rest of the branch and the current branch will be children
            logger->debug2("Connecting to 'top' of parent segment, so split the parent; connected_parent_end_map[chosen_parent] %i, parent_end %i; chosen_parent before %p",connected_parent_end_map[chosen_parent],parent_end, chosen_parent);
            logger->debug1("Parent size %i",chosen_parent->get_segment()->markers.size());

            CompositeBranchContainer *parent_of_parent = chosen_parent->get_parent();
            CompositeBranchContainer *new_before = chosen_parent->split_branch(1);
            new_before->set_parent(parent_of_parent);
            chosen_parent->set_parent(new_before);
            chosen_parent = new_before;

            logger->debug1("chosen parent after %p; parent size %i, new_before size %i",chosen_parent,chosen_parent->get_segment()->markers.size(),new_before->get_segment()->markers.size());
        }

        // Reverse direction of branch and segment based on current direction and side of new connection to parent
        // True if the segment is not reversed and the connection is at the bottom, OR if the segment is reversed and the connection is at the top
        if (branch->get_segment()->markers.size() > 1 && (branch->is_segment_reversed() ^ child_end == BOTTOM)){
            logger->debug2("reverse segment");
            branch->reverse_segment();
            child_end = chosen_connection->get_child_end();
        }

        // If this branch has children that have already connected but to the end this branch connects to its parent, split this branch and transfer the children to the new branch
        std::set<CompositeBranchContainer *> children_connected_wrong = connected_child_end_map[pair<CompositeBranchContainer*,BranchEnd>(branch,child_end)];
        if (children_connected_wrong.size() > 0){
            logger->debug1("Child connection wrong - need to split branch and connect child to the top of split");
            // Split first (has to take place after reversal (if needed) in order for split to be at correct location
            CompositeBranchContainer *new_before = branch->split_branch(1);
            branch->set_parent(new_before);
            
            // Transfer connection of children
            for (CompositeBranchContainer *child : children_connected_wrong){
                child->set_parent(new_before);
            }
            branch = new_before; // This will be the branch that now connects to the parent
        }
        
        // Set parent
        branch->set_parent(chosen_parent, true, child_end, parent_end);
        if (!chosen_parent){
            logger->debug2("chosen_parent is nullptr");
        }else{
            std::vector<NeuronSegment*> children = chosen_parent->get_segment()->child_list;
            logger->debug1("parent seg %p, child seg %p; parent's num_children %i, parent has child? %i",chosen_parent->get_segment(),branch->get_segment(), children.size(), std::find(children.begin(),children.end(),branch->get_segment())!=children.end());
        }
        
        
        // Mark the branch and its connection end (the end of the branch linked to its parent), so if a subsequent child connects at the other end the branch can be split
        if (branch->get_segment()->markers.size() > 1){ // Only if the branch has more than 1 marker
            connected_parent_end_map[branch] = child_end;
            logger->debug4("Setting connected_parent_end_map of %p to %i",branch,child_end);
        }
        
        // Mark the parent branch and its connection end in case when that parent connects it does so at the same end. That will require the parent to be split and the child branch to be moved to the top of the split.
        if (chosen_parent && chosen_parent->get_segment()->markers.size() > 1){
            connected_child_end_map[pair<CompositeBranchContainer *,BranchEnd>(chosen_parent,parent_end)].insert(branch);
        }
        
        /** Resolve connection_set: 2. Update connections at the other end of each branch to ensure the directionality of the branches set by this decision **/
        // ** No longer - split off top node instead as its own branch ** Remove all connections from the parent to its potential parents at the end connected to its child
        /*
        if (chosen_parent && chosen_parent->get_segment()->markers.size() > 1){
            ConnectionPtrSet child_connections;
            if (parent_end == TOP){
                //chosen_parent->remove_child_connections(BOTTOM);
                child_connections = chosen_parent->get_child_connections(BOTTOM);
            }
            else{
                //chosen_parent->remove_child_connections(TOP);
                child_connections = chosen_parent->get_child_connections(TOP);
            }
            
            //eliminated_connections.insert(child_connections.begin(),child_connections.end());
            // If there are any child_connections at the opposite end, split off first node of branch and redirect connections to new branch
            if (child_connections.size() > 0){
                for (Connection * child_conn : child_connections){
                    connect_to_top.insert(child_conn);
                }
            }
        }
         */
        
        // Remove all connections from the child to its potential children at the end connected to its parent
        /* The code immediately before removes the need to do this
        //child->remove_child_connections(chosen_connection->get_child_end());
        eliminated_connections.insert(child_connections.begin(),child_connections.end());
        */
        
        if (chosen_parent && processed_branches.find(chosen_parent) == processed_branches.end())
            branch = chosen_parent;
        else
            branch = nullptr;
    }
    logger->debug4("generate consensus 3");
    logger->debug4("root segment start point %f %f %f",c_root_segment->markers[0]->x,c_root_segment->markers[0]->y,c_root_segment->markers[0]->z);
    
    // Remove any root segment with fewer than 5 markers and no children, or a single child that jointly combine to fewer than 5 markers
    if (multiple_trees){
        for (int i = c_root_segments.size(); i > 0; i--){
            NeuronSegment *root_seg = c_root_segments[i-1];
            if (root_seg->child_list.size() == 0 && root_seg->markers.size() < 5 ||
                root_seg->child_list.size() == 1 && root_seg->child_list[0]->child_list.size() == 0 && root_seg->markers.size() + root_seg->child_list[0]->markers.size() < 5){
//                logger->info("i %i, root seg children %i, markers %i",i,root_seg->child_list.size(),root_seg->markers.size());
                CompositeBranchContainer *cb = get_branch_by_segment(root_seg);
//                logger->info("c_root branch children %i",cb->get_children().size());
//                remove(c_roots.begin(), c_roots.end(), get_branch_by_segment(root_seg));
//                remove(c_root_segments.begin(), c_root_segments.end(), root_seg);
                c_roots.erase(remove(c_roots.begin(), c_roots.end(), get_branch_by_segment(root_seg)), c_roots.end());
                c_root_segments.erase(remove(c_root_segments.begin(), c_root_segments.end(), root_seg), c_root_segments.end());

                if (root_seg == c_root_segment){
                    c_root = c_roots[0];
                    c_root_segment = c_root_segments[0];
                }
            }
        }
    }
//    c_roots.erase( c_roots.begin(),c_roots.end() );
//    c_root_segments.erase( c_root_segments.begin(),c_root_segments.end() );
    
    // Update marker head parent connection
    logger->debug4("Test update marker parents");
    for (CompositeBranchContainer *branch : get_branches()){
        if (branch->get_parent()){
            NeuronSegment *segment = branch->get_segment(), *parent_seg = branch->get_parent()->get_segment();
            // Link marker at top of segment to it's parent marker
            segment->markers[0]->parent = parent_seg->markers.back();
        }
    }
    logger->info("Exiting Composite::convert_to_consensus; number of roots %i, %i",c_roots.size(), c_root_segments.size());
};


void update_branch_tree_sizes(CompositeBranchContainer * branch, std::map<CompositeBranchContainer *,int> &branch_tree_size){
    int size = branch_tree_size[branch];
    branch = branch->get_parent();
    while(branch){
        branch_tree_size[branch] += size;
        branch = branch->get_parent();
    }
};
