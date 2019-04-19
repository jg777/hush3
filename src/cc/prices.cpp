/******************************************************************************
 * Copyright © 2014-2019 The SuperNET Developers.                             *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * SuperNET software, including this file may be copied, modified, propagated *
 * or distributed except according to the terms contained in the LICENSE file *
 *                                                                            *
 * Removal or modification of this copyright notice is prohibited.            *
 *                                                                            *
 ******************************************************************************/

#include "CCassets.h"
#include "CCPrices.h"

#define IS_CHARINSTR(c, str) (std::string(str).find((char)(c)) != std::string::npos)

/*
CBOPRET creates trustless oracles, which can be used for making a synthetic cash settlement system based on real world prices;
 
 0.5% fee based on betamount, NOT leveraged betamount!!
 0.1% collected by price basis determinant
 0.2% collected by rekt tx
 
 PricesBet -> +/-leverage, amount, synthetic -> opreturn includes current price
    funds are locked into 1of2 global CC address
    for first day, long basis is MAX(correlated,smoothed), short is MIN()
    reference price is the smoothed of the previous block
    if synthetic value + amount goes negative, then anybody can rekt it to collect a rektfee, proof of rekt must be included to show cost basis, rekt price
    original creator can liquidate at anytime and collect (synthetic value + amount) from globalfund
    0.5% of bet -> globalfund
  
 PricesStatus -> bettxid maxsamples returns initial params, cost basis, amount left, rekt:true/false, rektheight, initial synthetic price, current synthetic price, net gain
 
 PricesRekt -> bettxid height -> 0.1% to miner, rest to global CC
 
 PricesClose -> bettxid returns (synthetic value + amount)
 
 PricesList -> all bettxid -> list [bettxid, netgain]
 
 
*/

// helpers:

// returns true if there are only digits and no alphas or slashes in 's'
inline bool is_weight_str(std::string s) {
    return 
        std::count_if(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c); } ) > 0  &&
        std::count_if(s.begin(), s.end(), [](unsigned char c) { return std::isalpha(c) || c == '/'; } ) == 0;
}


// start of consensus code

CScript prices_betopret(CPubKey mypk,int32_t height,int64_t amount,int16_t leverage,int64_t firstprice,std::vector<uint16_t> vec,uint256 tokenid)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'B' << mypk << height << amount << leverage << firstprice << vec << tokenid);
    return(opret);
}

uint8_t prices_betopretdecode(CScript scriptPubKey,CPubKey &pk,int32_t &height,int64_t &amount,int16_t &leverage,int64_t &firstprice,std::vector<uint16_t> &vec,uint256 &tokenid)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    
    GetOpReturnData(scriptPubKey,vopret);
    if (vopret.size() > 2 && E_UNMARSHAL(vopret, ss >> e; ss >> f; ss >> pk; ss >> height; ss >> amount; ss >> leverage; ss >> firstprice; ss >> vec; ss >> tokenid) != 0 && e == EVAL_PRICES && f == 'B')
    {
        return(f);
    }
    return(0);
}

CScript prices_addopret(uint256 bettxid,CPubKey mypk,int64_t amount)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'A' << bettxid << mypk << amount);
    return(opret);
}

uint8_t prices_addopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int64_t &amount)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> amount) != 0 && e == EVAL_PRICES && f == 'A' )
    {
        return(f);
    }
    return(0);
}

CScript prices_costbasisopret(uint256 bettxid,CPubKey mypk,int32_t height,int64_t costbasis)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'C' << bettxid << mypk << height << costbasis);
    return(opret);
}

uint8_t prices_costbasisopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int32_t &height,int64_t &costbasis)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> pk; ss >> height; ss >> costbasis) != 0 && e == EVAL_PRICES && f == 'C' )
    {
        return(f);
    }
    return(0);
}

CScript prices_finalopret(uint256 bettxid,int64_t profits,int32_t height,CPubKey mypk,int64_t firstprice,int64_t costbasis,int64_t addedbets,int64_t positionsize,int16_t leverage)
{
    CScript opret;
    opret << OP_RETURN << E_MARSHAL(ss << EVAL_PRICES << 'F' << bettxid << profits << height << mypk << firstprice << costbasis << addedbets << positionsize << leverage);
    return(opret);
}

uint8_t prices_finalopretdecode(CScript scriptPubKey,uint256 &bettxid,int64_t &profits,int32_t &height,CPubKey &pk,int64_t &firstprice,int64_t &costbasis,int64_t &addedbets,int64_t &positionsize,int16_t &leverage)
{
    std::vector<uint8_t> vopret; uint8_t e,f;
    GetOpReturnData(scriptPubKey,vopret);
    if ( vopret.size() > 2 && E_UNMARSHAL(vopret,ss >> e; ss >> f; ss >> bettxid; ss >> profits; ss >> height; ss >> pk; ss >> firstprice; ss >> costbasis; ss >> addedbets; ss >> positionsize; ss >> leverage) != 0 && e == EVAL_PRICES && f == 'F' )
    {
        return(f);
    }
    return(0);
}

// price opret basic validation and retrieval
bool CheckPricesOpret(const CTransaction & tx, vscript_t &opret)
{
    return tx.vout.size() > 0 && GetOpReturnData(tx.vout.back().scriptPubKey, opret) && opret.size() > 2 && opret.begin()[0] == EVAL_PRICES && IS_CHARINSTR(opret.begin()[1], "BACF");
}

bool ValidateBetTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & bettx)
{
    uint256 tokenid;
    int64_t positionsize, firstprice;
    int32_t firstheight; 
    int16_t leverage;
    CPubKey pk, pricespk; 
    std::vector<uint16_t> vec;

    if (bettx.vout.size() < 5 || bettx.vout.size() > 6)
        return eval->Invalid("incorrect vout number for bet tx");

    vscript_t opret;
    if( prices_betopretdecode(bettx.vout.back().scriptPubKey, pk, firstheight, positionsize, leverage, firstprice, vec, tokenid) != 'B')
        return eval->Invalid("cannot decode opreturn for bet tx");

    pricespk = GetUnspendable(cp, 0);

    if (MakeCC1vout(cp->evalcode, bettx.vout[0].nValue, pk) != bettx.vout[0])
        return eval->Invalid("cannot validate vout0 in bet tx with pk from opreturn");
    if (MakeCC1vout(cp->evalcode, bettx.vout[1].nValue, pricespk) != bettx.vout[1])
        return eval->Invalid("cannot validate vout1 in bet tx with global pk");
    if( MakeCC1vout(cp->evalcode, bettx.vout[2].nValue, pricespk) != bettx.vout[2] )
        return eval->Invalid("cannot validate 1of2 vout2 in bet tx with pk from opreturn");

    return true;
}


bool ValidateAddFundingTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & addfundingtx)
{
    uint256 bettxid;
    int64_t amount;
    CPubKey pk, pricespk;

    if (addfundingtx.vout.size() < 3 || addfundingtx.vout.size() > 4)
        return eval->Invalid("incorrect vout number for add funding tx");

    vscript_t opret;
    if (prices_addopretdecode(addfundingtx.vout.back().scriptPubKey, bettxid, pk, amount) != 'A')
        return eval->Invalid("cannot decode opreturn for add funding tx");

    pricespk = GetUnspendable(cp, 0);

    if (MakeCC1vout(cp->evalcode, addfundingtx.vout[0].nValue, pk) != addfundingtx.vout[0])
        return eval->Invalid("cannot validate vout0 in add funding tx with pk from opreturn");
    if (MakeCC1vout(cp->evalcode, addfundingtx.vout[1].nValue, pricespk) != addfundingtx.vout[1])
        return eval->Invalid("cannot validate vout1 in add funding tx with global pk");

    return true;
}

bool ValidateCostbasisTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & costbasistx, const CTransaction & bettx)
{
    uint256 bettxid;
    int64_t amount;
    CPubKey pk, pricespk;
    int32_t height;

    // check basic structure:
    if (costbasistx.vout.size() < 3 || costbasistx.vout.size() > 4)
        return eval->Invalid("incorrect vout count for costbasis tx");

    vscript_t opret;
    if (prices_costbasisopretdecode(costbasistx.vout.back().scriptPubKey, bettxid, pk, height, amount) != 'C')
        return eval->Invalid("cannot decode opreturn for costbasis tx");

    pricespk = GetUnspendable(cp, 0);
    if (CTxOut(costbasistx.vout[0].nValue, CScript() << ParseHex(HexStr(pk)) << OP_CHECKSIG) != costbasistx.vout[0])  //might go to any pk who calculated costbasis
        return eval->Invalid("cannot validate vout0 in costbasis tx with pk from opreturn");
    if (MakeCC1vout(cp->evalcode, costbasistx.vout[1].nValue, pricespk) != costbasistx.vout[1])
        return eval->Invalid("cannot validate vout1 in costbasis tx with global pk");

    if (bettx.vout.size() < 1) // maybe this is already checked outside, but it is safe to check here too and have encapsulated check
        return eval->Invalid("incorrect bettx");

    // check costbasis rules:
    if (costbasistx.vout[0].nValue > bettx.vout[1].nValue / 10)
        return eval->Invalid("costbasis myfee too big");

    uint256 tokenid;
    int64_t positionsize, firstprice;
    int32_t firstheight;
    int16_t leverage;
    CPubKey betpk;
    std::vector<uint16_t> vec;
    if (prices_betopretdecode(bettx.vout.back().scriptPubKey, betpk, firstheight, positionsize, leverage, firstprice, vec, tokenid) != 'B')
        return eval->Invalid("cannot decode opreturn for bet tx");

    if( firstheight + PRICES_DAYWINDOW + PRICES_SMOOTHWIDTH > chainActive.Height() )
        return eval->Invalid("cannot calculate costbasis yet");

    return true;
}

bool ValidateFinalTx(struct CCcontract_info *cp, Eval *eval, const CTransaction & finaltx)
{
    uint256 bettxid;
    int64_t amount;
    CPubKey pk, pricespk;
    int64_t profits;
    int32_t height;
    int64_t firstprice, costbasis, addedbets, positionsize;
    int16_t leverage;

    if (finaltx.vout.size() < 2 || finaltx.vout.size() > 3)
        return eval->Invalid("incorrect vout number for final tx");

    vscript_t opret;
    if (prices_finalopretdecode(finaltx.vout.back().scriptPubKey, bettxid, profits, height, pk, firstprice, costbasis, addedbets, positionsize, leverage) != 'F')
        return eval->Invalid("cannot decode opreturn for final tx");

    pricespk = GetUnspendable(cp, 0);

    if (MakeCC1vout(cp->evalcode, finaltx.vout[0].nValue, pk) != finaltx.vout[0])
        return eval->Invalid("cannot validate vout0 in final tx with pk from opreturn");

    if( finaltx.vout.size() == 3 && MakeCC1vout(cp->evalcode, finaltx.vout[1].nValue, pricespk) != finaltx.vout[1] ) 
        return eval->Invalid("cannot validate vout1 in final tx with global pk");

    return true;
}

bool PricesValidate(struct CCcontract_info *cp,Eval* eval,const CTransaction &tx, uint32_t nIn)
{
    vscript_t vopret;


    if (strcmp(ASSETCHAINS_SYMBOL, "REKT0") == 0 && chainActive.Height() < 2100)
        return true;
    // check basic opret rules:
    if (!CheckPricesOpret(tx, vopret))
        return eval->Invalid("tx has no prices opreturn");

    uint8_t funcId = vopret.begin()[1];

    CTransaction vintx;
    vscript_t vintxOpret;
    int32_t ccVinCount = 0;
    int32_t prevoutN = 0;
    // load vintx (might be either bet or add funding tx):
    for (auto vin : tx.vin)
        if (cp->ismyvin(vin.scriptSig)) {
            uint256 hashBlock;
            if (!myGetTransaction(vin.prevout.hash, vintx, hashBlock))
                return eval->Invalid("cannot load vin tx");
            prevoutN = vin.prevout.n;
            ccVinCount++;
        }
    if (ccVinCount != 1)   // must be only one cc vintx
        return eval->Invalid("incorrect cc vin txns num");

    if (!CheckPricesOpret(vintx, vintxOpret))
        return eval->Invalid("cannot find prices opret in vintx");

    if (vintxOpret.begin()[1] == 'B' && prevoutN == 3) {   // check basic spending rules
        return eval->Invalid("cannot spend bet marker");
    }

    switch (funcId) {
    case 'B':   // bet 
        return eval->Invalid("unexpected validate for bet funcid");

    case 'A':   // add funding
        // check tx structure:
        if (!ValidateAddFundingTx(cp, eval, tx))
            return false;  // invalid state is already set in the func

        if (vintxOpret.begin()[1] == 'B') {
            if (!ValidateBetTx(cp, eval, vintx)) // check tx structure
                return false;  
        }
        else if (vintxOpret.begin()[1] == 'A') {
            // no need to validate the previous addfunding tx (it was validated when added)
        }

        if (prevoutN != 0) {   // check spending rules
            return eval->Invalid("incorrect vout to spend");
        }
        break;

    case 'C':   // set costbasis 
        if (!ValidateBetTx(cp, eval, vintx)) // first check bet tx 
            return false;  
        if (!ValidateCostbasisTx(cp, eval, tx, vintx))
            return false;  
        if (prevoutN != 1) {   // check spending rules
            return eval->Invalid("incorrect vout to spend");
        }
        //return eval->Invalid("test: costbasis is good");
        break;

    case 'F':   // final tx 
        if (!ValidateBetTx(cp, eval, vintx)) // first check bet tx 
            return false;
        if (!ValidateFinalTx(cp, eval, tx))
            return false;
        if (prevoutN != 2) {   // check spending rules
            return eval->Invalid("incorrect vout to spend");
        }
        break;

    default:
        return eval->Invalid("invalid funcid");
    }

    return true;
}
// end of consensus code

// helper functions for rpc calls in rpcwallet.cpp

int64_t AddPricesInputs(struct CCcontract_info *cp, CMutableTransaction &mtx, char *destaddr, int64_t total, int32_t maxinputs, uint256 vintxid, int32_t vinvout)
{
    int64_t nValue, price, totalinputs = 0; uint256 txid, hashBlock; std::vector<uint8_t> origpubkey; CTransaction vintx; int32_t vout, n = 0;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    SetCCunspents(unspentOutputs, destaddr);
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it = unspentOutputs.begin(); it != unspentOutputs.end(); it++)
    {
        txid = it->first.txhash;
        vout = (int32_t)it->first.index;
        if (vout == vinvout && txid == vintxid)
            continue;
        if (GetTransaction(txid, vintx, hashBlock, false) != 0 && vout < vintx.vout.size())
        {
            if ((nValue = vintx.vout[vout].nValue) >= total / maxinputs && myIsutxo_spentinmempool(ignoretxid, ignorevin, txid, vout) == 0)
            {
                if (total != 0 && maxinputs != 0)
                    mtx.vin.push_back(CTxIn(txid, vout, CScript()));
                nValue = it->second.satoshis;
                totalinputs += nValue;
                n++;
                if ((total > 0 && totalinputs >= total) || (maxinputs > 0 && n >= maxinputs))
                    break;
            }
        }
    }
    return(totalinputs);
}

UniValue prices_rawtxresult(UniValue &result, std::string rawtx, int32_t broadcastflag)
{
    CTransaction tx;
    if (rawtx.size() > 0)
    {
        result.push_back(Pair("hex", rawtx));
        if (DecodeHexTx(tx, rawtx) != 0)
        {
            if (broadcastflag != 0 && myAddtomempool(tx) != 0)
                RelayTransaction(tx);
            result.push_back(Pair("txid", tx.GetHash().ToString()));
            result.push_back(Pair("result", "success"));
        }
        else 
            result.push_back(Pair("error", "decode hex"));
    }
    else 
        result.push_back(Pair("error", "couldnt finalize CCtx"));
    return(result);
}

int32_t prices_syntheticvec(std::vector<uint16_t> &vec, std::vector<std::string> synthetic)
{
    int32_t i, need, ind, depth = 0; std::string opstr; uint16_t opcode, weight;
    if (synthetic.size() == 0) {
        std::cerr << "prices_syntheticvec() expression is empty" << std::endl;
        return(-1);
    }
    for (i = 0; i < synthetic.size(); i++)
    {
        need = 0;
        opstr = synthetic[i];
        if (opstr == "*")
            opcode = PRICES_MULT, need = 2;
        else if (opstr == "/")
            opcode = PRICES_DIV, need = 2;
        else if (opstr == "!")
            opcode = PRICES_INV, need = 1;
        else if (opstr == "**/")
            opcode = PRICES_MMD, need = 3;
        else if (opstr == "*//")
            opcode = PRICES_MDD, need = 3;
        else if (opstr == "***")
            opcode = PRICES_MMM, need = 3;
        else if (opstr == "///")
            opcode = PRICES_DDD, need = 3;
        else if (!is_weight_str(opstr) && (ind = komodo_priceind(opstr.c_str())) >= 0)
            opcode = ind, need = 0;
        else if ((weight = atoi(opstr.c_str())) > 0 && weight < KOMODO_MAXPRICES)
        {
            opcode = PRICES_WEIGHT | weight;
            need = 1;
        }
        else {
            std::cerr << "prices_syntheticvec() incorrect opcode=" << opstr << std::endl;
            return(-2);
        }
        if (depth < need) {
            std::cerr << "prices_syntheticvec() incorrect not enough operands for opcode=" << opstr << std::endl;
            return(-3);
        }
        depth -= need;
        std::cerr << "opcode=" << opcode << " opstr=" << opstr << " need=" << need << " depth=" << depth << std::endl;
        if ((opcode & KOMODO_PRICEMASK) != PRICES_WEIGHT) { // skip weight
            depth++;                                          // increase operands count
            std::cerr << "depth++=" << depth << std::endl;
        }
        if (depth > 3) {
            std::cerr << "prices_syntheticvec() to many operands, last=" << opstr << std::endl;
            return(-4);
        }
        vec.push_back(opcode);
    }
    if (depth != 0)
    {
        fprintf(stderr, "prices_syntheticvec() depth.%d not empty\n", depth);
        return(-5);
    }
    return(0);
}

// calculates price for synthetic expression
int64_t prices_syntheticprice(std::vector<uint16_t> vec, int32_t height, int32_t minmax, int16_t leverage)
{
    int32_t i, value, errcode, depth, retval = -1;
    uint16_t opcode;
    int64_t *pricedata, pricestack[4], price, den, a, b, c;

    pricedata = (int64_t *)calloc(sizeof(*pricedata) * 3, 1 + PRICES_DAYWINDOW * 2 + PRICES_SMOOTHWIDTH);
    price = den = depth = errcode = 0;

    for (i = 0; i < vec.size(); i++)
    {
        opcode = vec[i];
        value = (opcode & (KOMODO_MAXPRICES - 1));   // index or weight 
        std::cerr << "prices_syntheticprice" << " i=" << i << " price=" << price << " value=" << value << " depth=" << depth << std::endl;
        switch (opcode & KOMODO_PRICEMASK)
        {
        case 0: // indices 
            pricestack[depth] = 0;
            if (komodo_priceget(pricedata, value, height, 1) >= 0)
            {
                std::cerr << "prices_syntheticprice" << " pricedata[0]=" << pricedata[0] << " pricedata[1]=" << pricedata[1] << " pricedata[2]=" << pricedata[2] <<  " pricedata_int32[2]=" << *((uint32_t*)pricedata[2]) << std::endl;
                // push price to the prices stack
                if (!minmax)
                    pricestack[depth] = pricedata[2];   // use smoothed value if we are over 24h
                else
                {
                    // if we are within 24h use min or max price
                    if (leverage > 0)
                        pricestack[depth] = (pricedata[1] > pricedata[2]) ? pricedata[1] : pricedata[2]; // MAX
                    else
                        pricestack[depth] = (pricedata[1] < pricedata[2]) ? pricedata[1] : pricedata[2]; // MIN
                }
            }
            if (pricestack[depth] == 0)
                errcode = -1;

            depth++;
            break;

        case PRICES_WEIGHT: // multiply by weight and consume top of stack by updating price
            if (depth == 1) {
                depth--;
                price += pricestack[0] * value;
                den += value;     // acc weight value
            }
            else
                errcode = -2;
            break;

        case PRICES_MULT:
            if (depth >= 2) {
                b = pricestack[--depth];
                a = pricestack[--depth];
                pricestack[depth++] = (a * b) / SATOSHIDEN;
            }
            else
                errcode = -3;
            break;

        case PRICES_DIV:
            if (depth >= 2) {
                b = pricestack[--depth];
                a = pricestack[--depth];
                pricestack[depth++] = (a * SATOSHIDEN) / b;
            }
            else
                errcode = -4;
            break;

        case PRICES_INV:
            if (depth >= 1) {
                a = pricestack[--depth];
                pricestack[depth++] = (SATOSHIDEN * SATOSHIDEN) / a;
            }
            else
                errcode = -5;
            break;

        case PRICES_MDD:
            if (depth >= 3) {
                c = pricestack[--depth];
                b = pricestack[--depth];
                a = pricestack[--depth];
                pricestack[depth++] = (((a * SATOSHIDEN) / b) * SATOSHIDEN) / c;
            }
            else
                errcode = -6;
            break;

        case PRICES_MMD:
            if (depth >= 3) {
                c = pricestack[--depth];
                b = pricestack[--depth];
                a = pricestack[--depth];
                pricestack[depth++] = (a * b) / c;
            }
            else
                errcode = -7;
            break;

        case PRICES_MMM:
            if (depth >= 3) {
                c = pricestack[--depth];
                b = pricestack[--depth];
                a = pricestack[--depth];
                pricestack[depth++] = ((a * b) / SATOSHIDEN) * c;
            }
            else
                errcode = -8;
            break;

        case PRICES_DDD:
            if (depth >= 3) {
                c = pricestack[--depth];
                b = pricestack[--depth];
                a = pricestack[--depth];
                pricestack[depth++] = (((((SATOSHIDEN * SATOSHIDEN) / a) * SATOSHIDEN) / b) * SATOSHIDEN) / c;
            }
            else
                errcode = -9;
            break;

        default:
            errcode = -10;
            break;
        }
        if (errcode != 0)
            break;

        std::cerr << "pricestack[depth=" << depth << "]=" << pricestack[depth] << std::endl;

    }
    free(pricedata);

    if (errcode != 0)
        std::cerr << "prices_syntheticprice warning: errcode in switch=" << errcode << std::endl;

    if (den == 0) {
        std::cerr << "prices_syntheticprice den==0 return err=-11" << std::endl;
        return(-11);
    }
    else if (depth != 0) {
        std::cerr << "prices_syntheticprice depth!=0 err=-12" << std::endl;
        return(-12);
    }
    else if (errcode != 0) {
        std::cerr << "prices_syntheticprice err=" << errcode << std::endl;
        return(errcode);
    }
    std::cerr << "prices_syntheticprice price=" << price << " den=" << den << std::endl;
    return(price / den);
}

// calculates profit/loss for the bet
int64_t prices_syntheticprofits(int64_t &costbasis, int32_t firstheight, int32_t height, int16_t leverage, std::vector<uint16_t> vec, int64_t positionsize, int64_t addedbets, int64_t &price)
{
    int64_t profits = 0; 

    if (height < firstheight) {
        fprintf(stderr, "requested height is lower than bet firstheight.%d\n", height);
        return 0;
    }

    int32_t minmax = (height < firstheight + PRICES_DAYWINDOW);  // if we are within 24h then use min or max value 

    if ((price = prices_syntheticprice(vec, height, minmax, leverage)) < 0)
    {
        fprintf(stderr, "unexpected zero synthetic price at height.%d\n", height);
        return(0);
    }

    // clear lowest positions:
    price /= 10000;
    price *= 10000;
   
    if (minmax)    { // if we are within day window, set costbasis to max or min price value
        if (leverage > 0 && price > costbasis) {
            costbasis = price;  // set temp costbasis
            std::cerr << "prices_syntheticprofits() minmax costbasis=" << costbasis << " price=" << price << std::endl;
        }
        else if (leverage < 0 && (costbasis == 0 || price < costbasis)) {
            costbasis = price;
            std::cerr << "prices_syntheticprofits() minmax costbasis=" << costbasis << " price=" << price << std::endl;
        }
        else {  //-> use the previous value
            std::cerr << "prices_syntheticprofits() unchanged costbasis=" << costbasis << " price=" << price << " leverage=" << leverage << std::endl;
        }
            
    }
    else   {
        // use provided costbasis
        std::cerr << "prices_syntheticprofits() provided costbasis=" << costbasis << " price=" << price << std::endl;
        if (costbasis == 0)
            costbasis = price;
    }
    

    profits = costbasis > 0 ? ( ((price / 10000 * SATOSHIDEN) / costbasis) - SATOSHIDEN / 10000 ) : 0;
    std::cerr << "prices_syntheticprofits() (price /10000 * SATOSHIDEN)=" << (price /10000 * SATOSHIDEN) << std::endl;
    std::cerr << "prices_syntheticprofits() (price /10000 * SATOSHIDEN)/costbasis=" << (price /10000 * SATOSHIDEN)/costbasis << std::endl;

    std::cerr << "prices_syntheticprofits() profits1=" << profits << std::endl;
    //std::cerr << "prices_syntheticprofits() profits double=" << (double)price / (double)costbasis -1.0 << std::endl;
    //double dprofits = (double)price / (double)costbasis - 1.0;

    profits *= leverage * positionsize;
    //dprofits *= leverage * positionsize;
    std::cerr << "prices_syntheticprofits() profits2=" << profits << std::endl;
    //std::cerr << "prices_syntheticprofits() dprofits=" << dprofits << std::endl;


    return profits; //  (positionsize + addedbets + profits);
}

void prices_betjson(UniValue &result,int64_t profits,int64_t costbasis,int64_t positionsize,int64_t addedbets,int16_t leverage,int32_t firstheight,int64_t firstprice, int64_t lastprice)
{
    result.push_back(Pair("profits",ValueFromAmount(profits)));
    result.push_back(Pair("costbasis",ValueFromAmount(costbasis)));
    result.push_back(Pair("positionsize",ValueFromAmount(positionsize)));
    result.push_back(Pair("equity", ValueFromAmount(positionsize + addedbets + profits)));
    result.push_back(Pair("addedbets",ValueFromAmount(addedbets)));
    result.push_back(Pair("leverage",(int64_t)leverage));
    result.push_back(Pair("firstheight",(int64_t)firstheight));
    result.push_back(Pair("firstprice",ValueFromAmount(firstprice)));
    result.push_back(Pair("lastprice", ValueFromAmount(lastprice)));
}

// retrives costbasis from a tx spending bettx vout1
int64_t prices_costbasis(CTransaction bettx, uint256 &txidCostbasis)
{
    int64_t costbasis = 0;
    // if vout1 is spent, follow and extract costbasis from opreturn
    //uint8_t prices_costbasisopretdecode(CScript scriptPubKey,uint256 &bettxid,CPubKey &pk,int32_t &height,int64_t &costbasis)
    //uint256 txidCostbasis;
    int32_t vini;
    int32_t height;
    txidCostbasis = zeroid;

    if (CCgetspenttxid(txidCostbasis, vini, height, bettx.GetHash(), 1) < 0) {
        std::cerr << "prices_costbasis() no costbasis txid found" << std::endl;
        return 0;
    }

    CTransaction txCostbasis;
    uint256 hashBlock;
    uint256 bettxid;
    CPubKey pk;
    bool isLoaded = false;
    uint8_t funcId = 0;

    if ((isLoaded = myGetTransaction(txidCostbasis, txCostbasis, hashBlock)) &&
        txCostbasis.vout.size() > 0 &&
        (funcId = prices_costbasisopretdecode(txCostbasis.vout.back().scriptPubKey, bettxid, pk, height, costbasis)) != 0) {
        return costbasis;
    }

    std::cerr << "prices_costbasis() cannot load costbasis tx or decode opret" << " isLoaded=" << isLoaded <<  " funcId=" << (int)funcId << std::endl;
    return 0;
}

// calculates added bet total, returns the last baton txid
int64_t prices_batontxid(uint256 &batontxid, CTransaction bettx, uint256 bettxid)
{
    int64_t addedbets = 0;
    int32_t vini;
    int32_t height;
    int32_t retcode;

    batontxid = bettxid; // initially set to the source bet tx
    uint256 sourcetxid = bettxid;
    // iterate through batons, adding up vout1 -> addedbets
    while ((retcode = CCgetspenttxid(batontxid, vini, height, sourcetxid, 0)) == 0) {

        CTransaction txBaton;
        uint256 hashBlock;
        uint256 bettxid;
        CPubKey pk;
        bool isLoaded = false;
        uint8_t funcId = 0;
        int64_t amount;

        if ((isLoaded = myGetTransaction(batontxid, txBaton, hashBlock)) &&
            txBaton.vout.size() > 0 &&
            (funcId = prices_addopretdecode(txBaton.vout.back().scriptPubKey, bettxid, pk, amount)) != 0) {
            addedbets += amount;
            std::cerr << "prices_batontxid() added amount=" << amount << std::endl;
        }
        else {
            std::cerr << "prices_batontxid() cannot load or decode add bet tx, isLoaded=" << isLoaded << " funcId=" << (int)funcId << std::endl;
            return -1;
        }
        sourcetxid = batontxid;
    }

    return(addedbets);
}

UniValue PricesBet(int64_t txfee, int64_t amount, int16_t leverage, std::vector<std::string> synthetic)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C; 
    CPubKey pricespk, mypk; 
    int64_t betamount, firstprice; 
    std::vector<uint16_t> vec; 
    //char myaddr[64]; 
    std::string rawtx;

    if (leverage > PRICES_MAXLEVERAGE || leverage < -PRICES_MAXLEVERAGE)
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "leverage too big"));
        return(result);
    }
    cp = CCinit(&C, EVAL_PRICES);
    if (txfee == 0)
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp, 0);
    //GetCCaddress(cp, myaddr, mypk);
    if (prices_syntheticvec(vec, synthetic) < 0 || (firstprice = prices_syntheticprice(vec, nextheight - 1, 1, leverage)) < 0 || vec.size() == 0 || vec.size() > 4096)
    {
        result.push_back(Pair("result", "error"));
        result.push_back(Pair("error", "invalid synthetic"));
        return(result);
    }
    if (AddNormalinputs(mtx, mypk, amount + 5 * txfee, 64) >= amount + 5 * txfee)
    {
        betamount = (amount * 199) / 200;
        mtx.vout.push_back(MakeCC1vout(cp->evalcode, txfee, mypk)); // vout0 baton for total funding
        mtx.vout.push_back(MakeCC1vout(cp->evalcode, (amount - betamount) + 2 * txfee, pricespk));  // vout1, when spent, costbasis is set
        mtx.vout.push_back(MakeCC1vout(cp->evalcode, betamount, pricespk));
        mtx.vout.push_back(CTxOut(txfee, CScript() << ParseHex(HexStr(pricespk)) << OP_CHECKSIG)); // marker
        rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, prices_betopret(mypk, nextheight - 1, amount, leverage, firstprice, vec, zeroid));
        return(prices_rawtxresult(result, rawtx, 0));
    }
    result.push_back(Pair("result", "error"));
    result.push_back(Pair("error", "not enough funds"));
    return(result);
}

UniValue PricesAddFunding(int64_t txfee, uint256 bettxid, int64_t amount)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C; CTransaction bettx; 
    CPubKey pricespk, mypk; 
    //int64_t addedbets = 0, betamount, firstprice; 
    std::vector<uint16_t> vec; 
    uint256 batontxid; 
    std::string rawtx; 
    //char myaddr[64];

    cp = CCinit(&C, EVAL_PRICES);
    if (txfee == 0)
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp, 0);
    //GetCCaddress(cp, myaddr, mypk);
    if (AddNormalinputs(mtx, mypk, amount + 2*txfee, 64) >= amount + 2*txfee)
    {
        if (prices_batontxid(batontxid, bettx, bettxid) >= 0)
        {
            mtx.vin.push_back(CTxIn(batontxid, 0, CScript()));
            mtx.vout.push_back(MakeCC1vout(cp->evalcode, txfee, mypk)); // baton for total funding
            mtx.vout.push_back(MakeCC1vout(cp->evalcode, amount, pricespk));
            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, prices_addopret(bettxid, mypk, amount));
            return(prices_rawtxresult(result, rawtx, 0));
        }
        else
        {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "couldnt find batonttxid"));
            return(result);
        }
    }
    result.push_back(Pair("result", "error"));
    result.push_back(Pair("error", "not enough funds"));
    return(result);
}

// set cost basis (open price) for the bet
UniValue PricesSetcostbasis(int64_t txfee, uint256 bettxid)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nextheight);
    UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C; CTransaction bettx; uint256 hashBlock, batontxid, tokenid;
    int64_t myfee, positionsize = 0, addedbets, firstprice = 0, lastprice, profits = 0, costbasis = 0;
    int32_t i, firstheight = 0, height, numvouts; int16_t leverage = 0;
    std::vector<uint16_t> vec;
    CPubKey pk, mypk, pricespk; std::string rawtx;

    cp = CCinit(&C, EVAL_PRICES);
    if (txfee == 0)
        txfee = PRICES_TXFEE;

    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp, 0);
    if (myGetTransaction(bettxid, bettx, hashBlock) != 0 && (numvouts = bettx.vout.size()) > 3)
    {
        if (prices_betopretdecode(bettx.vout[numvouts - 1].scriptPubKey, pk, firstheight, positionsize, leverage, firstprice, vec, tokenid) == 'B')
        {
            if (nextheight <= firstheight + PRICES_DAYWINDOW + 1) {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "cannot calculate costbasis yet"));
                return(result);
            }

            addedbets = prices_batontxid(batontxid, bettx, bettxid);
            mtx.vin.push_back(CTxIn(bettxid, 1, CScript()));              // spend vin1 with betamount
            for (i = 0; i < PRICES_DAYWINDOW + 1; i++)   // the last datum for 24h is the costbasis value
            {
                if ((profits = prices_syntheticprofits(costbasis, firstheight, firstheight + i, leverage, vec, positionsize, addedbets, lastprice)) < 0)
                {   // we are in loss
                    result.push_back(Pair("rekt", (int64_t)1));
                    result.push_back(Pair("rektheight", (int64_t)firstheight + i));
                    break;
                }
            }
            if (i == PRICES_DAYWINDOW)
                result.push_back(Pair("rekt", 0));

            prices_betjson(result, profits, costbasis, positionsize, addedbets, leverage, firstheight, firstprice, lastprice);

            if (AddNormalinputs(mtx, mypk, txfee, 4) >= txfee)
            {
                myfee = bettx.vout[1].nValue / 10;   // fee for setting costbasis
                result.push_back(Pair("myfee", myfee));

                mtx.vout.push_back(CTxOut(myfee, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeCC1vout(cp->evalcode, bettx.vout[1].nValue - myfee, pricespk));
                rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, prices_costbasisopret(bettxid, mypk, firstheight + PRICES_DAYWINDOW /*- 1*/, costbasis));
                return(prices_rawtxresult(result, rawtx, 0));
            }
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "not enough funds"));
            return(result);
        }
    }
    result.push_back(Pair("result", "error"));
    result.push_back(Pair("error", "cant find bettxid"));
    return(result);
}

UniValue PricesRekt(int64_t txfee, uint256 bettxid, int32_t rektheight)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C; 
    CTransaction bettx; 
    uint256 hashBlock, tokenid, batontxid; 
    int64_t myfee = 0, positionsize, addedbets, firstprice, lastprice, profits, ignore, costbasis = 0; 
    int32_t firstheight, numvouts; 
    int16_t leverage; 
    std::vector<uint16_t> vec; 
    CPubKey pk, mypk, pricespk; 
    std::string rawtx;

    cp = CCinit(&C, EVAL_PRICES);
    if (txfee == 0)
        txfee = PRICES_TXFEE;
    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp, 0);
    if (myGetTransaction(bettxid, bettx, hashBlock) != 0 && (numvouts = bettx.vout.size()) > 3)
    {
        if (prices_betopretdecode(bettx.vout[numvouts - 1].scriptPubKey, pk, firstheight, positionsize, leverage, firstprice, vec, tokenid) == 'B')
        {
            uint256 costbasistxid;
            costbasis = prices_costbasis(bettx, costbasistxid);
            if (costbasis == 0) {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "costbasis not defined yet"));
                return(result);
            }

            addedbets = prices_batontxid(batontxid, bettx, bettxid);
            if ((profits = prices_syntheticprofits(costbasis /*ignore*/, firstheight, rektheight, leverage, vec, positionsize, addedbets, lastprice)) < 0)
            {
                myfee = (positionsize + addedbets) / 500;
            }
            prices_betjson(result, profits, costbasis, positionsize, addedbets, leverage, firstheight, firstprice, lastprice);
            if (myfee != 0)
            {
                mtx.vin.push_back(CTxIn(bettxid, 2, CScript()));
                mtx.vout.push_back(CTxOut(myfee, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
                mtx.vout.push_back(MakeCC1vout(cp->evalcode, bettx.vout[2].nValue - myfee - txfee, pricespk));
                rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, prices_finalopret(bettxid, profits, rektheight, mypk, firstprice, costbasis, addedbets, positionsize, leverage));
                return(prices_rawtxresult(result, rawtx, 0));
            }
            else
            {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "position not rekt"));
                return(result);
            }
        }
        else
        {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "cant decode opret"));
            return(result);
        }
    }
    result.push_back(Pair("result", "error"));
    result.push_back(Pair("error", "cant find bettxid"));
    return(result);
}

UniValue PricesCashout(int64_t txfee, uint256 bettxid)
{
    int32_t nextheight = komodo_nextheight();
    CMutableTransaction mtx = CreateNewContextualCMutableTransaction(Params().GetConsensus(), nextheight); UniValue result(UniValue::VOBJ);
    struct CCcontract_info *cp, C; char destaddr[64]; 
    CTransaction bettx; 
    uint256 hashBlock, batontxid, tokenid; 
    int64_t CCchange = 0, positionsize, inputsum, ignore, addedbets, firstprice, lastprice, profits, costbasis = 0; 
    int32_t i, firstheight, height, numvouts; 
    int16_t leverage; 
    std::vector<uint16_t> vec;
    CPubKey pk, mypk, pricespk; 
    std::string rawtx;

    cp = CCinit(&C, EVAL_PRICES);
    if (txfee == 0)
        txfee = PRICES_TXFEE;

    mypk = pubkey2pk(Mypubkey());
    pricespk = GetUnspendable(cp, 0);
    GetCCaddress(cp, destaddr, pricespk);
    if (myGetTransaction(bettxid, bettx, hashBlock) != 0 && (numvouts = bettx.vout.size()) > 3)
    {
        if (prices_betopretdecode(bettx.vout[numvouts - 1].scriptPubKey, pk, firstheight, positionsize, leverage, firstprice, vec, tokenid) == 'B')
        {
            uint256 costbasistxid;

            costbasis = prices_costbasis(bettx, costbasistxid);
            if (costbasis == 0) {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "costbasis not defined yet"));
                return(result);
            }

            addedbets = prices_batontxid(batontxid, bettx, bettxid);
            if ((profits = prices_syntheticprofits(costbasis, firstheight, nextheight - 1, leverage, vec, positionsize, addedbets, lastprice)) < 0)
            {
                prices_betjson(result, profits, costbasis, positionsize, addedbets, leverage, firstheight, firstprice, lastprice);
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "position rekt"));
                return(result);
            }
            prices_betjson(result, profits, costbasis, positionsize, addedbets, leverage, firstheight, firstprice, lastprice);
            mtx.vin.push_back(CTxIn(bettxid, 2, CScript()));
            if ((inputsum = AddPricesInputs(cp, mtx, destaddr, profits + txfee, 64, bettxid, 2)) > profits + txfee)
                CCchange = (inputsum - profits);
            mtx.vout.push_back(CTxOut(bettx.vout[2].nValue + profits, CScript() << ParseHex(HexStr(mypk)) << OP_CHECKSIG));
            if (CCchange >= txfee)
                mtx.vout.push_back(MakeCC1vout(cp->evalcode, CCchange, pricespk));
            rawtx = FinalizeCCTx(0, cp, mtx, mypk, txfee, prices_finalopret(bettxid, profits, nextheight - 1, mypk, firstprice, costbasis, addedbets, positionsize, leverage));
            return(prices_rawtxresult(result, rawtx, 0));
        }
        else
        {
            result.push_back(Pair("result", "error"));
            result.push_back(Pair("error", "cant decode opret"));
            return(result);
        }
    }
    return(result);
}

UniValue PricesInfo(uint256 bettxid, int32_t refheight)
{
    UniValue result(UniValue::VOBJ);
    CTransaction bettx;
    uint256 hashBlock, batontxid, tokenid;
    int64_t myfee, ignore = 0, positionsize = 0, addedbets = 0, firstprice = 0, lastprice, profits = 0, costbasis = 0;
    int32_t i, firstheight = 0, height, numvouts;
    int16_t leverage = 0;
    std::vector<uint16_t> vec;
    CPubKey pk, mypk, pricespk;
    std::string rawtx;
    uint256 costbasistxid;

    if (myGetTransaction(bettxid, bettx, hashBlock) != 0 && (numvouts = bettx.vout.size()) > 3)
    {
        if (prices_betopretdecode(bettx.vout[numvouts - 1].scriptPubKey, pk, firstheight, positionsize, leverage, firstprice, vec, tokenid) == 'B')
        {
            if (refheight > 0 && refheight < firstheight) {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "incorrect height"));
                return(result);
            }
            if (refheight == 0)
                refheight = komodo_nextheight()-1;

            costbasis = prices_costbasis(bettx, costbasistxid);
            addedbets = prices_batontxid(batontxid, bettx, bettxid);

            /*
            if( costbasis == 0 && prices_syntheticprofits(true, costbasis, firstheight, firstheight, leverage, vec, positionsize, addedbets) < 0) {
                result.push_back(Pair("result", "error"));
                result.push_back(Pair("error", "cannot calculate costbasis"));
                return(result);
            } */

            if ((profits = prices_syntheticprofits(costbasis, firstheight, refheight, leverage, vec, positionsize, addedbets, lastprice)) < 0)
            {
                result.push_back(Pair("rekt", 1));
                result.push_back(Pair("rektfee", (positionsize + addedbets) / 500));
            }
            else
                result.push_back(Pair("rekt", 0));
            result.push_back(Pair("batontxid", batontxid.GetHex()));
            if(!costbasistxid.IsNull())
                result.push_back(Pair("costbasistxid", costbasistxid.GetHex()));
            prices_betjson(result, profits, costbasis, positionsize, addedbets, leverage, firstheight, firstprice, lastprice);
            result.push_back(Pair("height", (int64_t)refheight));
            return(result);
        }
    }
    result.push_back(Pair("result", "error"));
    result.push_back(Pair("error", "cant find bettxid"));
    return(result);
}

UniValue PricesList()
{
    UniValue result(UniValue::VARR); std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex; 
    struct CCcontract_info *cp, C; 
    int64_t amount, firstprice; int32_t height; int16_t leverage; uint256 txid, hashBlock, tokenid; 
    CPubKey pk, pricespk; 
    std::vector<uint16_t> vec; 
    CTransaction vintx; 

    cp = CCinit(&C, EVAL_PRICES);
    pricespk = GetUnspendable(cp, 0);
    SetCCtxids(addressIndex, cp->normaladdr, false);
    for (std::vector<std::pair<CAddressIndexKey, CAmount> >::const_iterator it = addressIndex.begin(); it != addressIndex.end(); it++)
    {
        txid = it->first.txhash;
        if (GetTransaction(txid, vintx, hashBlock, false) != 0)
        {
            if (vintx.vout.size() > 0 && prices_betopretdecode(vintx.vout[vintx.vout.size() - 1].scriptPubKey, pk, height, amount, leverage, firstprice, vec, tokenid) == 'B')
            {
                result.push_back(txid.GetHex());
            }
        }
    }
    return(result);
}


