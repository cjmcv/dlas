/*!
* \brief Tensor. 
*/

#include "tensor.hpp"
#include "util/logger.hpp"

namespace ecas {

Tensor::Tensor(std::vector<int> &shape) {
    it_ = new ITensor();
    it_->shape = shape;

    size_ = 1;
    for (int i=0; i < shape.size(); i++) {
        size_ *= shape[i];
    }
    if (size_ == 0)
        std::abort();
    
    buffer_ = new Buffer(size_);
    it_->data = (char *)buffer_->data();

    it_->mem_type = MEMORY_HOST;
}

// Tensor::Tensor(ITensor &in) {
//     size_ = 1;
//     for (int i=0; i < in.shape.size(); i++) {
//         size_ *= in.shape[i];
//     }
//     if (size_ == 0)
//         std::abort();

//     shape_ = in.shape;
//     buffer_ = new Buffer(in.data, size_);
// }

Tensor::~Tensor() {
    delete buffer_;
}

void Tensor::CopyFrom(ITensor &in) {
    // Check dimension.
    for (int i=0; i<it_->shape.size(); i++) {
        if (it_->shape[i] != in.shape[i]) {
            ECAS_LOGE("Tensor::CloneFrom -> shape mismatch.\n");
            exit(-1);
        }
    }
    // Check memory type,
    if (it_->mem_type != in.mem_type) {
        ECAS_LOGE("Tensor::CloneFrom -> memory type mismatch.\n");
        exit(-1);
    }

    memcpy(it_->data, in.data, size_);
}

void Tensor::CopyTo(ITensor *out) {
    // Check dimension.
    for (int i=0; i<it_->shape.size(); i++) {
        if (it_->shape[i] != out->shape[i]) {
            ECAS_LOGE("Tensor::CopyTo -> shape mismatch.\n");
            exit(-1);
        }
    }
    // Check memory type,
    if (it_->mem_type != out->mem_type) {
        ECAS_LOGE("Tensor::CopyTo -> memory type mismatch.\n");
        exit(-1);
    }

    memcpy(out->data, it_->data, size_);
}

}  // end of namespace ecas.