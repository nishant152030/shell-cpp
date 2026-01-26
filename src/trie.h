#pragma once
#include <string>
#include <vector>
#include <map>
namespace Trie {
    class TrieNode
    {
    public:
        bool isLeaf;
        std::map<char, TrieNode*> children;
        TrieNode();
        ~TrieNode();
    };
    
    void insert(TrieNode* root, const std::string &key);

    bool search(TrieNode* root, const std::string &key);

    bool isPrefix(TrieNode* root, const std::string &key);

    std::pair<std::string,std::vector<std::string>> autoComplete(TrieNode* root, const std::string &key);
}

