#ifndef _CIRCUIT_H_
#define _CIRCUIT_H_

#include "Constants.h"
#include "Data.h"

#include "ethsnarks.hpp"
#include "utils.hpp"
#include "jubjub/point.hpp"
#include "jubjub/eddsa.hpp"
#include "gadgets/longsightl.hpp"
#include "gadgets/merkle_tree.hpp"
#include "gadgets/sha256_many.hpp"

using namespace ethsnarks;


namespace Loopring
{

class LeqGadget : public GadgetT
{
public:
    VariableT lt = make_variable(pb, 1, "lt");
    VariableT leq = make_variable(pb, 1, "leq");
    libsnark::comparison_gadget<ethsnarks::FieldT> comparison;

    LeqGadget(
        ProtoboardT& pb,
        const VariableT& A,
        const VariableT& B,
        const std::string& annotation_prefix
    ) :
        GadgetT(pb, annotation_prefix),

        lt(make_variable(pb, 1, "lt")),
        leq(make_variable(pb, 1, "leq")),
        comparison(pb, 128, A, B, lt, leq, "A <= B")
    {

    }

    const VariableT result() const
    {
        return leq;
    }

    void generate_r1cs_witness()
    {
        comparison.generate_r1cs_witness();
    }

    void generate_r1cs_constraints()
    {
        comparison.generate_r1cs_constraints();
        pb.add_r1cs_constraint(ConstraintT(leq, 1, 1));
    }
};

class RateCheckerGadget : public GadgetT
{
public:
    VariableT fillS;
    VariableT fillB;
    VariableT amountS;
    VariableT amountB;

    VariableT invariant;

    RateCheckerGadget(
        ProtoboardT& pb,
        const VariableT& _fillS,
        const VariableT& _fillB,
        const VariableT& _amountS,
        const VariableT& _amountB,
        const std::string& annotation_prefix
    ) :
        GadgetT(pb, annotation_prefix),

        fillS(_fillS),
        fillB(_fillB),
        amountS(_amountS),
        amountB(_amountB),

        invariant(make_variable(pb, ".invariant"))
    {

    }

    void generate_r1cs_witness()
    {
        pb.val(invariant) = pb.val(amountS) * pb.val(fillB);
    }

    void generate_r1cs_constraints()
    {
        pb.add_r1cs_constraint(ConstraintT(amountS, fillB, invariant));
        pb.add_r1cs_constraint(ConstraintT(amountB, fillS, invariant));
    }
};

class OrderGadget : public GadgetT
{
public:

    libsnark::dual_variable_gadget<FieldT> owner;
    libsnark::dual_variable_gadget<FieldT> tokenS;
    libsnark::dual_variable_gadget<FieldT> tokenB;
    libsnark::dual_variable_gadget<FieldT> tokenF;
    libsnark::dual_variable_gadget<FieldT> amountS;
    libsnark::dual_variable_gadget<FieldT> amountB;
    libsnark::dual_variable_gadget<FieldT> amountF;

    const jubjub::VariablePointT publicKey;

    // variables for signature
    const jubjub::VariablePointT sig_R;
    const VariableArrayT sig_s;
    const VariableArrayT sig_m;
    jubjub::PureEdDSA_Verify signatureVerifier;

    OrderGadget(
        ProtoboardT& pb,
        const jubjub::Params& params,
        const std::string& annotation_prefix
    ) :
        GadgetT(pb, annotation_prefix),

        owner(pb, 160, FMT(annotation_prefix, ".owner")),
        tokenS(pb, 160, FMT(annotation_prefix, ".tokenS")),
        tokenB(pb, 160, FMT(annotation_prefix, ".tokenB")),
        tokenF(pb, 161, FMT(annotation_prefix, ".tokenF")),
        amountS(pb, 128, FMT(annotation_prefix, ".amountS")),
        amountB(pb, 128, FMT(annotation_prefix, ".amountB")),
        amountF(pb, 128, FMT(annotation_prefix, ".amountF")),

        publicKey(pb, FMT(annotation_prefix, ".publicKey")),

        sig_R(pb, FMT(annotation_prefix, ".R")),
        sig_s(make_var_array(pb, FieldT::size_in_bits(), FMT(annotation_prefix, ".s"))),
        sig_m(flatten({owner.bits, tokenS.bits, tokenB.bits, tokenF.bits, amountS.bits, amountB.bits, amountF.bits})),

        signatureVerifier(pb, params, jubjub::EdwardsPoint(params.Gx, params.Gy), publicKey, sig_R, sig_s, sig_m, FMT(annotation_prefix, ".signatureVerifier"))
    {

    }

    void generate_r1cs_witness(const Order& order)
    {
        owner.bits.fill_with_bits_of_field_element(this->pb, order.owner);
        owner.generate_r1cs_witness_from_bits();

        tokenS.bits.fill_with_bits_of_field_element(this->pb, order.tokenS);
        tokenS.generate_r1cs_witness_from_bits();
        tokenB.bits.fill_with_bits_of_field_element(this->pb, order.tokenB);
        tokenB.generate_r1cs_witness_from_bits();
        tokenF.bits.fill_with_bits_of_field_element(this->pb, order.tokenF);
        tokenF.generate_r1cs_witness_from_bits();

        amountS.bits.fill_with_bits_of_field_element(this->pb, order.amountS);
        amountS.generate_r1cs_witness_from_bits();
        amountB.bits.fill_with_bits_of_field_element(this->pb, order.amountB);
        amountB.generate_r1cs_witness_from_bits();
        amountF.bits.fill_with_bits_of_field_element(this->pb, order.amountF);
        amountF.generate_r1cs_witness_from_bits();

        this->pb.val(publicKey.x) = order.publicKey.x;
        this->pb.val(publicKey.y) = order.publicKey.y;

        this->pb.val(sig_R.x) = order.sig.R.x;
        this->pb.val(sig_R.y) = order.sig.R.y;
        sig_s.fill_with_bits_of_field_element(this->pb, order.sig.s);
        signatureVerifier.generate_r1cs_witness();
    }

    void generate_r1cs_constraints()
    {
        owner.generate_r1cs_constraints(true);
        tokenS.generate_r1cs_constraints(true);
        tokenB.generate_r1cs_constraints(true);
        tokenF.generate_r1cs_constraints(true);
        amountS.generate_r1cs_constraints(true);
        amountB.generate_r1cs_constraints(true);
        amountF.generate_r1cs_constraints(true);

        // TODO: Check public key in order message
        signatureVerifier.generate_r1cs_constraints();
    }
};

class UpdateFilledGadget : public GadgetT
{
public:
    typedef merkle_path_authenticator<LongsightL12p5_MP_gadget> MerklePathCheckT;
    typedef markle_path_compute<LongsightL12p5_MP_gadget> MerklePathT;

    const VariableT merkleRootBefore;

    libsnark::dual_variable_gadget<FieldT> fill;

    VariableT filledBefore;
    VariableT filledAfter;

    LongsightL12p5_MP_gadget leafBefore;
    LongsightL12p5_MP_gadget leafAfter;

    const VariableArrayT proof;
    MerklePathCheckT proofVerifierBefore;
    MerklePathT rootCalculatorAfter;

    UpdateFilledGadget(
        ProtoboardT& pb,
        const VariableT& _merkleRoot,
        const VariableArrayT& address,
        const std::string& annotation_prefix
    ) :
        GadgetT(pb, annotation_prefix),

        merkleRootBefore(_merkleRoot),

        fill(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fill")),

        filledBefore(make_variable(pb, FMT(annotation_prefix, ".filledBefore"))),
        filledAfter(make_variable(pb, FMT(annotation_prefix, ".filledAfter"))),

        leafBefore(pb, libsnark::ONE, {filledBefore, filledBefore}, FMT(annotation_prefix, ".leafBefore")),
        leafAfter(pb, libsnark::ONE, {filledAfter, filledAfter}, FMT(annotation_prefix, ".leafAfter")),

        proof(make_var_array(pb, TREE_DEPTH_FILLED, FMT(annotation_prefix, ".proof"))),
        proofVerifierBefore(pb, TREE_DEPTH_FILLED, address, merkle_tree_IVs(pb), leafBefore.result(), merkleRootBefore, proof, FMT(annotation_prefix, ".pathBefore")),
        rootCalculatorAfter(pb, TREE_DEPTH_FILLED, address, merkle_tree_IVs(pb), leafAfter.result(), proof, FMT(annotation_prefix, ".pathAfter"))
    {

    }

    const VariableT result() const
    {
        return rootCalculatorAfter.result();
    }

    const VariableT getFilledAfter() const
    {
        return filledAfter;
    }

    void generate_r1cs_witness(const ethsnarks::FieldT& filled, const std::vector<ethsnarks::FieldT>& _proof, const libsnark::dual_variable_gadget<FieldT>& fill)
    {
        this->pb.val(filledBefore) = filled;
        this->pb.val(filledAfter) = this->pb.val(filledBefore) + this->pb.val(fill.packed);

        leafBefore.generate_r1cs_witness();
        leafAfter.generate_r1cs_witness();

        proof.fill_with_field_elements(this->pb, _proof);
        proofVerifierBefore.generate_r1cs_witness();
        rootCalculatorAfter.generate_r1cs_witness();
    }

    void generate_r1cs_constraints(const libsnark::dual_variable_gadget<FieldT>& fill)
    {
        this->pb.add_r1cs_constraint(ConstraintT(filledBefore + fill.packed, 1, filledAfter), "filledBefore + fill = filledAfter");

        leafBefore.generate_r1cs_constraints();
        leafAfter.generate_r1cs_constraints();

        proofVerifierBefore.generate_r1cs_constraints();
        rootCalculatorAfter.generate_r1cs_constraints();
    }
};

class RingSettlementGadget : public GadgetT
{
public:
    const VariableT merkleRoot;

    OrderGadget orderA;
    OrderGadget orderB;

    libsnark::dual_variable_gadget<FieldT> fillS_A;
    libsnark::dual_variable_gadget<FieldT> fillB_A;
    libsnark::dual_variable_gadget<FieldT> fillF_A;
    libsnark::dual_variable_gadget<FieldT> fillS_B;
    libsnark::dual_variable_gadget<FieldT> fillB_B;
    libsnark::dual_variable_gadget<FieldT> fillF_B;

    UpdateFilledGadget updateFilledA;
    UpdateFilledGadget updateFilledB;

    LeqGadget filledLeqA;
    LeqGadget filledLeqB;

    RateCheckerGadget rateCheckerA;
    RateCheckerGadget rateCheckerB;

    RateCheckerGadget rateCheckerFeeA;
    RateCheckerGadget rateCheckerFeeB;

    LeqGadget matchLeqA;
    LeqGadget matchLeqB;

    RingSettlementGadget(
        ProtoboardT& pb,
        const jubjub::Params& params,
        const VariableT& _merkleRoot,
        const std::string& annotation_prefix
    ) :
        GadgetT(pb, annotation_prefix),

        orderA(pb, params, FMT(annotation_prefix, ".orderA")),
        orderB(pb, params, FMT(annotation_prefix, ".orderB")),

        fillS_A(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fillS_A")),
        fillB_A(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fillB_A")),
        fillF_A(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fillF_A")),
        fillS_B(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fillS_B")),
        fillB_B(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fillB_B")),
        fillF_B(pb, NUM_BITS_AMOUNT, FMT(annotation_prefix, ".fillF_B")),

        merkleRoot(_merkleRoot),
        updateFilledA(pb, merkleRoot, orderA.signatureVerifier.m_hash_RAM.result(), FMT(annotation_prefix, ".updateFilledA")),
        updateFilledB(pb, updateFilledA.result(), orderB.signatureVerifier.m_hash_RAM.result(), FMT(annotation_prefix, ".updateFilledB")),

        filledLeqA(pb, updateFilledA.getFilledAfter(), orderA.amountS.packed, FMT(annotation_prefix, ".filled_A < .amountSA")),
        filledLeqB(pb, updateFilledB.getFilledAfter(), orderB.amountS.packed, FMT(annotation_prefix, ".filled_B < .amountSB")),

        rateCheckerA(pb, fillS_A.packed, fillB_A.packed, orderA.amountS.packed, orderA.amountB.packed, FMT(annotation_prefix, ".rateA")),
        rateCheckerB(pb, fillS_B.packed, fillB_B.packed, orderA.amountB.packed, orderB.amountB.packed, FMT(annotation_prefix, ".rateB")),

        rateCheckerFeeA(pb, fillF_A.packed, fillS_A.packed, orderA.amountF.packed, orderA.amountS.packed, FMT(annotation_prefix, ".rateFeeA")),
        rateCheckerFeeB(pb, fillF_B.packed, fillS_B.packed, orderA.amountF.packed, orderB.amountS.packed, FMT(annotation_prefix, ".rateFeeB")),

        matchLeqA(pb, fillB_B.packed, fillS_A.packed, FMT(annotation_prefix, ".fillB_B <= .fillS_A")),
        matchLeqB(pb, fillB_A.packed, fillS_B.packed, FMT(annotation_prefix, ".fillB_A <= .fillS_B"))
    {

    }

    const VariableT result() const
    {
        return updateFilledB.result();
    }

    const std::vector<VariableArrayT> getTransfers() const
    {
        return {orderA.tokenS.bits, orderA.owner.bits, orderB.owner.bits, fillS_A.bits,
                orderA.tokenF.bits, orderA.owner.bits, orderA.owner.bits, fillF_A.bits,
                orderB.tokenS.bits, orderB.owner.bits, orderA.owner.bits, fillS_B.bits,
                orderB.tokenF.bits, orderB.owner.bits, orderB.owner.bits, fillF_B.bits};
    }

    void generate_r1cs_witness (const RingSettlement& ringSettlement)
    {
        orderA.generate_r1cs_witness(ringSettlement.ring.orderA);
        orderB.generate_r1cs_witness(ringSettlement.ring.orderB);

        fillS_A.bits.fill_with_bits_of_field_element(this->pb, ringSettlement.ring.fillS_A);
        fillS_A.generate_r1cs_witness_from_bits();
        fillB_A.bits.fill_with_bits_of_field_element(this->pb, ringSettlement.ring.fillB_A);
        fillB_A.generate_r1cs_witness_from_bits();
        fillF_A.bits.fill_with_bits_of_field_element(this->pb, ringSettlement.ring.fillF_A);
        fillF_A.generate_r1cs_witness_from_bits();
        fillS_B.bits.fill_with_bits_of_field_element(this->pb, ringSettlement.ring.fillS_B);
        fillS_B.generate_r1cs_witness_from_bits();
        fillB_B.bits.fill_with_bits_of_field_element(this->pb, ringSettlement.ring.fillB_B);
        fillB_B.generate_r1cs_witness_from_bits();
        fillF_B.bits.fill_with_bits_of_field_element(this->pb, ringSettlement.ring.fillF_B);
        fillF_B.generate_r1cs_witness_from_bits();

        // Update merkle tree
        this->pb.val(merkleRoot) = ringSettlement.merkleRoot;
        updateFilledA.generate_r1cs_witness(ringSettlement.filledA, ringSettlement.proofA, fillS_A);
        updateFilledB.generate_r1cs_witness(ringSettlement.filledB, ringSettlement.proofB, fillS_B);

        filledLeqA.generate_r1cs_witness();
        filledLeqB.generate_r1cs_witness();

        //
        // Matching
        //

        // Check rates
        rateCheckerA.generate_r1cs_witness();
        rateCheckerB.generate_r1cs_witness();
        rateCheckerFeeA.generate_r1cs_witness();
        rateCheckerFeeB.generate_r1cs_witness();

        // Check settlement
        matchLeqA.generate_r1cs_witness();
        matchLeqB.generate_r1cs_witness();
    }


    void generate_r1cs_constraints()
    {
        orderA.generate_r1cs_constraints();
        orderB.generate_r1cs_constraints();

        fillS_A.generate_r1cs_constraints(true);
        fillB_A.generate_r1cs_constraints(true);
        fillF_A.generate_r1cs_constraints(true);
        fillS_B.generate_r1cs_constraints(true);
        fillB_B.generate_r1cs_constraints(true);
        fillF_B.generate_r1cs_constraints(true);

        updateFilledA.generate_r1cs_constraints(fillS_A);
        updateFilledB.generate_r1cs_constraints(fillS_B);

        filledLeqA.generate_r1cs_constraints();
        filledLeqB.generate_r1cs_constraints();

        //
        // Matching
        //

        // Check if tokenS/tokenB match
        pb.add_r1cs_constraint(ConstraintT(orderA.tokenS.packed, 1, orderB.tokenB.packed));
        pb.add_r1cs_constraint(ConstraintT(orderA.tokenB.packed, 1, orderB.tokenS.packed));

        // Check rates
        rateCheckerA.generate_r1cs_constraints();
        rateCheckerB.generate_r1cs_constraints();
        rateCheckerFeeA.generate_r1cs_constraints();
        rateCheckerFeeB.generate_r1cs_constraints();

        // Check settlement
        matchLeqA.generate_r1cs_constraints();
        matchLeqB.generate_r1cs_constraints();
    }
};

class CircuitGadget : public GadgetT
{
public:

    unsigned int numRings;
    jubjub::Params params;
    std::vector<RingSettlementGadget> ringSettlements;

    VariableT merkleRoot;

    std::vector<VariableArrayT> transfers;
    VariableArrayT publicData;

    sha256_many* publicDataHash;

    CircuitGadget(ProtoboardT& pb, const std::string& annotation_prefix) : GadgetT(pb, annotation_prefix)
    {
        this->publicDataHash = nullptr;
    }

    ~CircuitGadget()
    {
        if (publicDataHash)
        {
            delete publicDataHash;
        }
    }

    void build(int numRings)
    {
        this->numRings = numRings;

        merkleRoot = make_variable(pb, "merkleRoot");
        for( size_t j = 0; j < numRings; j++ )
        {
            VariableT ringMerkleRoot = (j == 0) ? merkleRoot : ringSettlements.back().result();
            ringSettlements.emplace_back(pb, params, ringMerkleRoot, FMT("ringSettlement"));

            // Store transfers from ring settlement
            std::vector<VariableArrayT> ringTransfers = ringSettlements.back().getTransfers();
            transfers.insert(transfers.end(), ringTransfers.begin(), ringTransfers.end());
        }

        for( auto& ringSettlement : ringSettlements )
        {
            ringSettlement.generate_r1cs_constraints();
        }

        // Check public data
        publicData = flatten(transfers);
        publicDataHash = new sha256_many(pb, publicData, ".publicDataHash");
        publicDataHash->generate_r1cs_constraints();
    }

    void printInfo()
    {
        std::cout << pb.num_constraints() << " constraints (" << (pb.num_constraints() / numRings) << "/ring)" << std::endl;
    }

    bool generateWitness(const std::vector<Loopring::RingSettlement>& ringSettlementsData)
    {
        for(unsigned int i = 0; i < ringSettlementsData.size(); i++)
        {
            ringSettlements[i].generate_r1cs_witness(ringSettlementsData[i]);
        }

        publicDataHash->generate_r1cs_witness();

        // Print out calculated hash of transfer data
        auto full_output_bits = publicDataHash->result().get_digest();
        uint8_t full_output_bytes[32];
        bv_to_bytes(full_output_bits, full_output_bytes);
        char hexstr[65];
        hexstr[64] = '\0';
        for(int i = 0; i < full_output_bits.size()/8; i++)
        {
            sprintf(hexstr + i*2, "%02x", full_output_bytes[i]);
        }
        std::cout << "Public input hash: " << hexstr << std::endl;

        return true;
    }
};

}

#endif
