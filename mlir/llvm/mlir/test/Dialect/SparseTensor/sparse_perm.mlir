// NOTE: Assertions have been autogenerated by utils/generate-test-checks.py
// RUN: mlir-opt %s -sparsification | FileCheck %s

#X = #sparse_tensor.encoding<{
  map = (d0, d1, d2) -> (d2 : dense, d0 : dense, d1 : dense)
}>

#trait = {
  indexing_maps = [
    affine_map<(i,j,k) -> (k,i,j)>,  // A (in)
    affine_map<(i,j,k) -> (i,j,k)>   // X (out)
  ],
  iterator_types = ["parallel", "parallel", "parallel"]
}

// CHECK-LABEL:   func @sparse_static_dims(
// CHECK-SAME:                          %[[VAL_0:.*]]: tensor<10x20x30xf32, #sparse_tensor.encoding<{{{.*}}}>>,
// CHECK-SAME:                          %[[VAL_1:.*]]: tensor<20x30x10xf32>) -> tensor<20x30x10xf32> {
// CHECK-DAG:       %[[ZERO:.*]] = arith.constant 0.000000e+00 : f32
// CHECK-DAG:       %[[VAL_2:.*]] = arith.constant 20 : index
// CHECK-DAG:       %[[VAL_3:.*]] = arith.constant 30 : index
// CHECK-DAG:       %[[VAL_4:.*]] = arith.constant 10 : index
// CHECK-DAG:       %[[VAL_5:.*]] = arith.constant 0 : index
// CHECK-DAG:       %[[VAL_6:.*]] = arith.constant 1 : index
// CHECK-DAG:       %[[VAL_7:.*]] = sparse_tensor.values %[[VAL_0]] : tensor<10x20x30xf32, #sparse_tensor.encoding<{{{.*}}}>>
// CHECK-DAG:       %[[VAL_9:.*]] = bufferization.to_memref %[[VAL_1]] : memref<20x30x10xf32>
// CHECK:           linalg.fill ins(%[[ZERO]] : f32) outs(%[[VAL_9]] : memref<20x30x10xf32>)
// CHECK:           scf.for %[[VAL_10:.*]] = %[[VAL_5]] to %[[VAL_3]] step %[[VAL_6]] {
// CHECK:             scf.for %[[VAL_11:.*]] = %[[VAL_5]] to %[[VAL_4]] step %[[VAL_6]] {
// CHECK:               %[[VAL_12:.*]] = arith.muli %[[VAL_10]], %[[VAL_4]] : index
// CHECK:               %[[VAL_13:.*]] = arith.addi %[[VAL_12]], %[[VAL_11]] : index
// CHECK:               scf.for %[[VAL_14:.*]] = %[[VAL_5]] to %[[VAL_2]] step %[[VAL_6]] {
// CHECK:                 %[[VAL_15:.*]] = arith.muli %[[VAL_13]], %[[VAL_2]] : index
// CHECK:                 %[[VAL_16:.*]] = arith.addi %[[VAL_15]], %[[VAL_14]] : index
// CHECK:                 %[[VAL_17:.*]] = memref.load %[[VAL_7]]{{\[}}%[[VAL_16]]] : memref<?xf32>
// CHECK:                 memref.store %[[VAL_17]], %[[VAL_9]]{{\[}}%[[VAL_14]], %[[VAL_10]], %[[VAL_11]]] : memref<20x30x10xf32>
// CHECK:               }
// CHECK:             }
// CHECK:           }
// CHECK:           %[[VAL_18:.*]] = bufferization.to_tensor %[[VAL_9]] : memref<20x30x10xf32>
// CHECK:           return %[[VAL_18]] : tensor<20x30x10xf32>
// CHECK:         }
func.func @sparse_static_dims(%arga: tensor<10x20x30xf32, #X>,
                              %argx: tensor<20x30x10xf32>) -> tensor<20x30x10xf32> {
  %0 = linalg.generic #trait
    ins(%arga: tensor<10x20x30xf32, #X>)
    outs(%argx: tensor<20x30x10xf32>) {
      ^bb(%a : f32, %x: f32):
        linalg.yield %a : f32
  } -> tensor<20x30x10xf32>
  return %0 : tensor<20x30x10xf32>
}

// CHECK-LABEL:   func @sparse_dynamic_dims(
// CHECK-SAME:                          %[[VAL_0:.*]]: tensor<?x?x?xf32, #sparse_tensor.encoding<{{{.*}}}>>,
// CHECK-SAME:                          %[[VAL_1:.*]]: tensor<?x?x?xf32>) -> tensor<?x?x?xf32> {
// CHECK-DAG:       %[[ZERO:.*]] = arith.constant 0.000000e+00 : f32
// CHECK-DAG:       %[[VAL_2:.*]] = arith.constant 2 : index
// CHECK-DAG:       %[[VAL_3:.*]] = arith.constant 0 : index
// CHECK-DAG:       %[[VAL_4:.*]] = arith.constant 1 : index
// CHECK-DAG:       %[[VAL_5:.*]] = sparse_tensor.values %[[VAL_0]] : tensor<?x?x?xf32, #sparse_tensor.encoding<{{{.*}}}>>
// CHECK-DAG:       %[[VAL_6:.*]] = tensor.dim %[[VAL_0]], %[[VAL_2]] : tensor<?x?x?xf32, #sparse_tensor.encoding<{{{.*}}}>>
// CHECK-DAG:       %[[VAL_7:.*]] = tensor.dim %[[VAL_0]], %[[VAL_3]] : tensor<?x?x?xf32, #sparse_tensor.encoding<{{{.*}}}>>
// CHECK-DAG:       %[[VAL_8:.*]] = tensor.dim %[[VAL_0]], %[[VAL_4]] : tensor<?x?x?xf32, #sparse_tensor.encoding<{{{.*}}}>>
// CHECK-DAG:       %[[VAL_10:.*]] = bufferization.to_memref %[[VAL_1]] : memref<?x?x?xf32>
// CHECK:           linalg.fill ins(%[[ZERO]] : f32) outs(%[[VAL_10]] : memref<?x?x?xf32>)
// CHECK:           scf.for %[[VAL_11:.*]] = %[[VAL_3]] to %[[VAL_6]] step %[[VAL_4]] {
// CHECK:             scf.for %[[VAL_12:.*]] = %[[VAL_3]] to %[[VAL_7]] step %[[VAL_4]] {
// CHECK:               %[[VAL_13:.*]] = arith.muli %[[VAL_7]], %[[VAL_11]] : index
// CHECK:               %[[VAL_14:.*]] = arith.addi %[[VAL_13]], %[[VAL_12]] : index
// CHECK:               scf.for %[[VAL_15:.*]] = %[[VAL_3]] to %[[VAL_8]] step %[[VAL_4]] {
// CHECK:                 %[[VAL_16:.*]] = arith.muli %[[VAL_8]], %[[VAL_14]] : index
// CHECK:                 %[[VAL_17:.*]] = arith.addi %[[VAL_16]], %[[VAL_15]] : index
// CHECK:                 %[[VAL_18:.*]] = memref.load %[[VAL_5]]{{\[}}%[[VAL_17]]] : memref<?xf32>
// CHECK:                 memref.store %[[VAL_18]], %[[VAL_10]]{{\[}}%[[VAL_15]], %[[VAL_11]], %[[VAL_12]]] : memref<?x?x?xf32>
// CHECK:               }
// CHECK:             }
// CHECK:           }
// CHECK:           %[[VAL_19:.*]] = bufferization.to_tensor %[[VAL_10]] : memref<?x?x?xf32>
// CHECK:           return %[[VAL_19]] : tensor<?x?x?xf32>
// CHECK:         }
func.func @sparse_dynamic_dims(%arga: tensor<?x?x?xf32, #X>,
                               %argx: tensor<?x?x?xf32>) -> tensor<?x?x?xf32> {
  %0 = linalg.generic #trait
    ins(%arga: tensor<?x?x?xf32, #X>)
    outs(%argx: tensor<?x?x?xf32>) {
      ^bb(%a : f32, %x: f32):
        linalg.yield %a : f32
  } -> tensor<?x?x?xf32>
  return %0 : tensor<?x?x?xf32>
}
