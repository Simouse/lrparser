#include "syntax.h"

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "grammar.h"
#include "src/automata/automata.h"
#include "src/common.h"
#include "src/util/bitset.h"
#include "src/util/formatter.h"

using std::vector;

namespace gram {

// Helper of automaton creation from LR0 grammar,
// which generates a description for the state. e.g.
// S -> aB : S0 = {S -> .aB, S -> a.B, S -> aB.}
static String createStateNameLR0(const gram::Grammar &G, const gram::Symbol &N,
                                 const std::vector<int> &productionBody,
                                 int dotPos) {
    assert(static_cast<size_t>(dotPos) <= productionBody.size());

    const auto &symbols = G.getAllSymbols();
    String s = N.name;
    s += " ->";

    int i = 0;
    for (; i < dotPos; ++i) {
        s += ' ';
        s += symbols[productionBody[i]].name;
    }
    s += ' ';
    s += Grammar::SignStrings::dot;
    for (; i < static_cast<int>(productionBody.size()); ++i) {
        s += ' ';
        s += symbols[productionBody[i]].name;
    }
    return s;
}

void SyntaxAnalysisSLR::buildNFA() {
    Automaton &M = this->nfa;
    const Grammar &G = this->gram;

    util::Formatter fmt;
    const auto &symbols = G.getAllSymbols();
    //    const auto &prodVecTable = G.getProductions();

    // Store links here, so we can demonstrate how to add epsilon
    // transitions from each state which wants N to all states
    // that generates N eventually.
    // { stateid => nid }
    std::unordered_map<StateID, int> epsilonLinks;

    // Store states which begin the given symbol's productions.
    // { nid => vector<stateid> }
    std::unordered_map<int, std::vector<StateID>> prodStartStates;

    // Store states which finish the given symbol's productions.
    // { nid => vector<stateid> }
    std::unordered_map<int, std::vector<StateID>> prodFinishStates;

    // First we need to copy all actions by order, so symbols and
    // actions are paired.
    for (auto const &symbol : symbols) {
        M.addAction(symbol.name);
    }

    // States from grammar
    //    for (const auto &entry : prodVecTable) {
    //        // Model
    //        // { S -> a B | c, B -> b } ==> prodVecTable
    //        //   S -> a B | c           ==> entry
    //        //   S                      ==> nid
    //        // { a B, c }               ==> prodVec
    //        //   a B                    ==> prod
    //        const auto &prodVec = entry.second;
    //        int nid = entry.first;
    //
    //        for (const auto &prod : prodVec) {
    //            vector<StateID> stateIds(prod.size() + 1);
    //            for (int dotPos = 0; dotPos <= prod.size(); ++dotPos) {
    //                String name = createStateNameLR0(G, symbols[nid], prod,
    //                dotPos); StateID stateId = M.addState(name);
    //                stateIds[dotPos] = stateId;
    //            }
    //            // Link states
    //            for (int dotPos = 0; dotPos < prod.size(); ++dotPos) {
    //                StateID thisState{stateIds[dotPos]};
    //                StateID nextState{stateIds[dotPos + 1]};
    //                auto &nextSymbol = symbols[prod[dotPos]];
    //                M.addTransition(thisState, nextState,
    //                ActionID{nextSymbol.id});
    //
    //                // Which non-terminal symbol do I want to link to?
    //                if (nextSymbol.type == gram::SymbolType::NON_TERM) {
    //                    epsilonLinks.emplace(thisState, nextSymbol.id);
    //                }
    //            }
    //
    //            // Which states can provide my desired symbol?
    //            prodStartStates[nid].push_back(stateIds.front());
    //            prodFinishStates[nid].push_back(stateIds.back());
    //
    //            reduceMap[stateIds.back()] = ?;
    //        }
    //    }

    auto const &productionTable = G.getProductionTable();
    for (int i = 0; i < static_cast<int>(productionTable.size()); ++i) {
        ProductionID productionId{i};
        auto const &production = productionTable[i];
        auto const &rightSymbols = production.rightSymbols;
        vector<StateID> stateIds(rightSymbols.size() + 1);
        for (int dotPos = 0; dotPos <= static_cast<int>(rightSymbols.size());
             ++dotPos) {
            String name = createStateNameLR0(G, symbols[production.leftSymbol],
                                             rightSymbols, dotPos);
            StateID stateId = M.addState(name);
            stateIds[dotPos] = stateId;
        }
        // Link states
        for (int dotPos = 0; dotPos < static_cast<int>(rightSymbols.size());
             ++dotPos) {
            StateID thisState{stateIds[dotPos]};
            StateID nextState{stateIds[dotPos + 1]};
            auto &nextSymbol = symbols[rightSymbols[dotPos]];
            M.addTransition(thisState, nextState, ActionID{nextSymbol.id});

            // Which non-terminal symbol do I want to link to?
            if (nextSymbol.type == gram::SymbolType::NON_TERM) {
                epsilonLinks.emplace(thisState, nextSymbol.id);
            }
        }

        // Which states can provide my desired symbol?
        prodStartStates[production.leftSymbol].push_back(stateIds.front());
        prodFinishStates[production.leftSymbol].push_back(stateIds.back());

        // Record states with their reducible productions
        reduceMap[stateIds.back()] = productionId;
    }

    // display(DisplayType::AUTOMATON, "Generate states from each grammar rule",
    //         &M);

    // Start state: Create S' for S
    // Since name cannot contain a `'` in it, `S'` is legit
    StateID extStart{-1}, extEnd{-1};
    {
        auto dotSign = Grammar::SignStrings::dot;
        auto &start = G.getStartSymbol();
        String name = start.name + '\'';
        StateID s0 = M.addState(name + " -> " + dotSign + " " + start.name);
        M.markStartState(s0);
        StateID s1 = M.addState(name + " -> " + start.name + " " + dotSign);
        M.addTransition(s0, s1, ActionID{start.id});
        M.markStartState(s0);
        epsilonLinks.emplace(s0, start.id);
        extStart = s0;
        extEnd = s1;
    }

    // Save id of NFA final state of augmented grammar
    this->extendedEnd = extEnd;

    // display(DisplayType::AUTOMATON, "Create a new state for start symbol",
    // &M);

    // Mark final states
    // The last state of augmented start symbol production is a final state.
    M.markFinalState(extEnd);

    // If a non-terminal has a `$` in its follow set,
    // all productions which generate this symbol will be marked final.
    for (auto const &symbol : symbols) {
        if (symbol.followSet.find(G.getEndOfInputSymbol().id) ==
            symbol.followSet.end()) {
            continue;
        }
        // So this symbol can be accepted.
        // Mark all states that can already generate this symbol.
        for (auto state : prodFinishStates[symbol.id]) {
            M.markFinalState(state);
        }
    }

    // display(DisplayType::AUTOMATON, "Mark final states", &M);

    // Creating epsilon edge for each symbol
    for (const auto &link : epsilonLinks) {
        StateID from = link.first;
        for (StateID to : prodStartStates.at(link.second)) {
            M.addEpsilonTransition(from, to);
        }
        // display(
        //     DisplayType::AUTOMATON,
        //     fmt.formatView("Create epsilon transitions from state %-3d : %s",
        //                    from, M.getAllStates()[from].label.c_str())
        //         .data(), // Formatter's string is NULL-terminated so it's
        //         safe
        //     &M);
    }

    display(DisplayType::AUTOMATON, "NFA is built", &M);
}

void SyntaxAnalysisSLR::buildDFA() {
    dfa = nfa.toDFA();
    display(DisplayType::AUTOMATON, "DFA is built", &dfa);
}

void SyntaxAnalysisSLR::buildParseTables() {
    // Now DFA has complete information
    // 1. DFA has epsilon action in its action vector, so we need to skip it.
    // 2. DFA doesn't have a '$' action, and we need to process it specially.

    auto const &states = dfa.getAllStates();
    auto const &statesBitmap = dfa.getStatesBitmap();
    auto const &formerStates = dfa.getFormerStates();
    auto const &symbols = gram.getAllSymbols();
    auto const &productions = gram.getProductionTable();
    // auto const &actionReceivers = dfa.getActionReceivers();

    auto nActions = static_cast<int>(dfa.getAllActions().size());
    //    auto nNFAStates = static_cast<int>(formerStates.size());
    auto nDFAStates = static_cast<int>(states.size());

    // Shift and Goto items
    for (const auto &state : states) {
        auto const &trans = state.trans;
        for (auto const &entry : trans) {
            ActionID action{entry.first};
            StateID nextState{entry.second};
            ParseAction item{(symbols[action].type == SymbolType::NON_TERM)
                                 ? ParseAction::GOTO
                                 : ParseAction::SHIFT,
                             nextState};
            addParseTableEntry(state.id, action, item);
        }
    }

    // Process "reduce"
    // Bitset for reducible NFA states (so we can know if a DFA state is
    // reducible)
    util::BitSet mask{
        static_cast<util::BitSet::size_type>(formerStates.size())};

    for (auto const &entry : reduceMap) {
        mask.add(entry.first);
    }

    // for (int i = 0; i < nNFAStates; ++i) {
    //     if (formerStates[i].reduceAction >= 0)
    //         mask.add(i);
    // }

    for (StateID state{0}; state < nDFAStates; state = StateID{state + 1}) {
        auto dfaStateBitmap = statesBitmap[state]; // Copy a bitmap
        dfaStateBitmap &= mask;
        for (auto s : dfaStateBitmap) {
            auto nfaState = static_cast<StateID>(s);
            for (ActionID action{0}; action < nActions;
                 action = ActionID{action + 1}) {
                // Check if the action is allowed by followSet
                ProductionID product = reduceMap.at(nfaState);
                int reducedSymbolID = productions[product].leftSymbol;
                auto const &symbol = symbols[reducedSymbolID];
                if (symbol.followSet.find(action) != symbol.followSet.end()) {
                    addParseTableEntry(
                        state, action,
                        ParseAction{ParseAction::REDUCE, reducedSymbolID});
                }
            }
        }
    }

    // Process end of input
    auto endOfInput = ActionID{gram.getEndOfInputSymbol().id};
    for (int i = 0; i < nDFAStates; ++i) {
        if (statesBitmap[i].contains(this->extendedEnd)) {
            addParseTableEntry(StateID{i}, endOfInput,
                               ParseAction{ParseAction::SUCCESS, -1});
        }
    }
}

// StateID SyntaxAnalysisSLR::getExtEnd() const {
//     return extendedEnd;
// }

void SyntaxAnalysisSLR::addParseTableEntry(StateID state, ActionID act,
                                           ParseAction pact) {
    if (actionTable.empty()) {
        actionTable = std::vector<std::vector<std::vector<ParseAction>>>(
            dfa.getAllStates().size(),
            std::vector<std::vector<ParseAction>>(gram.getAllSymbols().size()));
    }
    actionTable[state][act].push_back(pact);
}

SyntaxAnalysisLR::ParseTable const &SyntaxAnalysisSLR::getActionTable() const {
    return actionTable;
}

gram::Grammar const &SyntaxAnalysisSLR::getGrammer() const { return gram; }

gram::Automaton const &SyntaxAnalysisSLR::getDFA() const { return dfa; }

String SyntaxAnalysisSLR::dumpParseTableEntry(StateID state,
                                              ActionID action) const {

    auto const &items = actionTable.at(state).at(action);
    String s;
    bool commaFlag = false;
    for (auto const &item : items) {
        if (commaFlag)
            s += ',';
        commaFlag = true;
        using Type = ParseAction::Type;
        switch (item.type) {
        case Type::SUCCESS:
            s += "acc";
            break;
        case Type::GOTO:
            s += std::to_string(item.dest);
            break;
        case Type::SHIFT:
            s += 's';
            s += std::to_string(item.dest);
            break;
        case Type::REDUCE:
            s += 'r';
            s += std::to_string(item.action);
            break;
        default:
            throw std::runtime_error(
                "dumpParseTableEntry(): Unknown parse action type");
        }
    }
    return s;
}

} // namespace gram