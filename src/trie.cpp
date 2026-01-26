#include "trie.h"
#include <iostream>

namespace Trie {
    TrieNode::TrieNode() {
        isLeaf = false;
    }
    TrieNode::~TrieNode() {
        for(auto &pr: children){
            delete pr.second;
        }
        children.clear();
    }

    void insert(TrieNode* root, const std::string &key) {
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children.find(c) == curr->children.end()) {
                TrieNode* new_node = new TrieNode();
                curr->children[c] = new_node;
            } 
            curr = curr->children[c];
        }
        curr->isLeaf = true;
    }

    bool search(TrieNode* root, const std::string &key) {
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children.find(c) == curr->children.end()) return false;
            curr = curr->children[c];
        }
        return curr->isLeaf;
    }

    bool isPrefix(TrieNode* root, const std::string &key) {
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children.find(c) == curr->children.end()) return false;
            curr = curr->children[c];
        }
        return true;
    }

    void findAllWords(TrieNode* node, std::string currendWord, std::vector<std::string> &words) {
        if(node->isLeaf){
            words.push_back(currendWord);
        }
        for(auto &[c,nextNode]: node->children) {
            findAllWords(nextNode, currendWord + c, words);
        }
    }

    std::vector<std::string> autoComplete(TrieNode* root, const std::string &key){
        TrieNode* curr = root;
        for(const char &c: key) {
            if(curr->children.find(c) == curr->children.end()) return {};
            curr = curr->children[c];
        }
        std::vector<std::string> words = {};
        findAllWords(curr, "", words);
        return words;
    }
}