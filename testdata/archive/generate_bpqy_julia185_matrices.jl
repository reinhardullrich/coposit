#!/usr/bin/env julia

# Materialize the published Julia 1.8.5 generators without their solver-only package dependencies.
using LinearAlgebra
using Random

length(ARGS) == 1 || error("usage: julia generate_bpqy_julia185_matrices.jl OUTPUT.tsv")
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

function generate_spn(seed, x)
    Random.seed!(seed)
    n = length(x)
    support = count(value -> value > 1e-8, x)
    zero = n - support
    r = psd_block(n)
    nonnegative = zeros(n, n)
    nonnegative[1:support, support + 1:n] = uniform(3.0, support, zero)
    nonnegative[support + 1:n, 1:support] = nonnegative[1:support, support + 1:n]'
    nonnegative[support + 1:n, support + 1:n] = Symmetric(uniform(3.0, zero, zero))
    return transform_and_reorder(r + nonnegative, x)
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

function support_two_minimum(matrix)
    best = minimum(diag(matrix))
    for i in 1:size(matrix, 1), j in i + 1:size(matrix, 1)
        curvature = matrix[i, i] - 2matrix[i, j] + matrix[j, j]
        t = curvature > 0 ? clamp((matrix[j, j] - matrix[i, j]) / curvature, 0.0, 1.0) : 0.0
        value = t^2 * matrix[i, i] + 2t * (1 - t) * matrix[i, j] + (1 - t)^2 * matrix[j, j]
        best = min(best, value)
    end
    return best
end

# These two published rho=2 optima guard the RNG calls and numerical construction order.
for (seed, expected) in ((0, 0.143704042198936), (1, 0.269541396676984))
    x = designated_solution(25, 6, seed)
    q = generate_psd(seed, x)
    isapprox(support_two_minimum(q), expected; atol=2e-13, rtol=2e-13) || error("published PSD validation failed at seed $seed")
end

open(ARGS[1], "w") do output
    println(output, "class\tdimension\trho0\tseed\tprimitive_upper")
    for class in ("COP", "PSD", "SPN"), n in (25, 50), rho0 in (round(Int, n / 4), round(Int, n / 2), round(Int, 3n / 4)), seed in 0:24
        x = designated_solution(n, rho0, seed)
        matrix = class == "COP" ? generate_cop(seed, x) : class == "PSD" ? generate_psd(seed, x) : generate_spn(seed, x)
        println(output, join((class, n, rho0, seed, join(primitive_upper(matrix), ',')), '\t'))
    end
end
