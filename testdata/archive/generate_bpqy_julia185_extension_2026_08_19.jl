#!/usr/bin/env julia

# Extend the published Julia 1.8.5 Float64 construction to five additional orders.
using LinearAlgebra
using Random

length(ARGS) == 1 || error("usage: julia generate_bpqy_julia185_extension_2026_08_19.jl OUTPUT.tsv")
BLAS.set_num_threads(1)

uniform(scale, dimensions...) = scale .* rand(dimensions...)

function designated_solution(n, rho0, seed)
    Random.seed!(seed)
    while true
        positive = rand(rho0)
        positive ./= sum(positive)
        minimum(positive) >= 1e-8 || continue
        return shuffle([positive; zeros(n - rho0)])
    end
end

function psd_block(n)
    eigenvalues = Diagonal(uniform(3.0, n))
    basis, _ = qr(rand(-5:5, n, n))
    return basis * eigenvalues * basis'
end

function transform_and_reorder(block, x)
    positive = findall(value -> value > 1e-8, x)
    zero = findall(value -> value <= 1e-8, x)
    permutation = [positive; zero]
    transform = I - ones(length(x)) * x[permutation]'
    q = transform * block * transform'
    inverse = sortperm(permutation)
    return q[inverse, inverse]
end

function generate_psd(seed, x)
    Random.seed!(seed)
    return transform_and_reorder(psd_block(length(x)), x)
end

function generate_cop(seed, x)
    Random.seed!(seed)
    n = length(x)
    support = count(value -> value > 1e-8, x)
    zero = n - support
    zero >= 5 || error("designated solution needs at least five zero coordinates")
    horn = [1 -1 1 1 -1; -1 1 -1 1 1; 1 -1 1 -1 1; 1 1 -1 1 -1; -1 1 1 -1 1]
    c = uniform(1.0, zero - 5, 5)
    b = psd_block(zero - 5)
    rbb = [b c; c' horn]
    raa = psd_block(support)
    f = [7 4.32 0 0 4.32; 4.32 7 4.32 0 0; 0 4.32 7 4.32 0; 0 0 4.32 7 4.32; 4.32 0 0 4.32 7]
    error_bound = 0.99 * (-dot(horn, f) / sum(f))
    raa = raa / opnorm(raa, 2) * error_bound
    block = [raa zeros(support, zero); zeros(zero, support) rbb]
    return transform_and_reorder(block, x)
end

function primitive_upper(matrix)
    rationals = [rationalize(BigInt, matrix[i, j]; tol=0) for i in axes(matrix, 1) for j in i:size(matrix, 2)]
    common_denominator = foldl(lcm, Base.denominator.(rationals); init=big(1))
    integers = [numerator(value) * div(common_denominator, Base.denominator(value)) for value in rationals]
    divisor = foldl(gcd, abs.(integers); init=big(0))
    return div.(integers, divisor)
end

open(ARGS[1], "w") do output
    println(output, "class\tdimension\trho0\tseed\tprimitive_upper")
    for class in ("COP", "PSD"), n in (20, 30, 35, 40, 45),
            rho0 in (round(Int, n / 4), round(Int, n / 2), round(Int, 3n / 4)), seed in 1:25
        x = designated_solution(n, rho0, seed)
        matrix = class == "COP" ? generate_cop(seed, x) : generate_psd(seed, x)
        println(output, join((class, n, rho0, seed, join(primitive_upper(matrix), ',')), '\t'))
    end
end
