/******************************************************************//**
 * \file   algorithms.hpp
 * \author Elliot Goodrich
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *********************************************************************/

#ifndef INCLUDE_GUARD_12E75493_BA12_4EF3_B0D8_92747A030D0E
#define INCLUDE_GUARD_12E75493_BA12_4EF3_B0D8_92747A030D0E

#include <type_traits>
#include <utility>

namespace vta {

// predicates

/** is_homogenous */
template <typename... Args>
struct is_homogenous;

template <typename First, typename Second, typename... Args>
struct is_homogenous<First, Second, Args...> {
	static bool const value = std::is_same<First, Second>::value
	                       && is_homogenous<Second, Args...>::value;
};

template <typename Arg>
struct is_homogenous<Arg> : public std::true_type {};

template <>
struct is_homogenous<> : public std::true_type {};

/** is_homogenous_after */
template <template<class> class Transform, typename... Args>
struct is_homogenous_after;

template <template<class> class Transform, typename First, typename Second, typename... Args>
struct is_homogenous_after<Transform, First, Second, Args...> {
	static bool const value = std::is_same<typename Transform<First>::type, typename Transform<Second>::type>::value
	                       && is_homogenous_after<Transform, Second, Args...>::value;
};

template <template<class> class Transform, typename Arg>
struct is_homogenous_after<Transform, Arg> : public std::true_type {};

template <template<class> class Transform>
struct is_homogenous_after<Transform> : public std::true_type {};

/** is_unique */
template <int... Ns>
struct is_unique;

template <>
struct is_unique<> {
	static bool const value = true;
};

template <int N>
struct is_unique<N> {
	static bool const value = true;
};

template <int M, int N, int... Ns>
struct is_unique<M, N, Ns...> {
	static bool const value = (M != N) && is_unique<M, Ns...>::value && is_unique<N, Ns...>::value;
};

// Forward after
template <typename Function, typename Transformation>
class forward_after_f {
	Function mF;

public:
	constexpr forward_after_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename... Args>
	constexpr auto operator()(Args&&... args) const {
		return Transformation::transform(mF, std::forward<Args>(args)...);
	}

	template <typename... Args>
	auto operator()(Args&&... args) {
		return Transformation::transform(mF, std::forward<Args>(args)...);
	}
};

template <typename Transformation, typename Function>
constexpr forward_after_f<Function, Transformation> forward_after(Function&& f) {
	return {std::forward<Function>(f)};
}

/**************************************************************************************************
 * Transformations                                                                                *
 **************************************************************************************************/

namespace detail {

template <typename Function, typename... Transforms>
class compose_helper_f;

template <typename Function, typename FirstTransform, typename... Transforms>
class compose_helper_f<Function, FirstTransform, Transforms...> {
	Function mF;

public:
	constexpr compose_helper_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename... Args>
	constexpr auto operator()(Args&&... args) const {
		return FirstTransform::transform(compose_helper_f<Function, Transforms...>{mF},
		                                 std::forward<Args>(args)...);
	}

	template <typename... Args>
	auto operator()(Args&&... args) {
		return FirstTransform::transform(compose_helper_f<Function, Transforms...>{mF},
		                                 std::forward<Args>(args)...);
	}
};

template <typename Function>
class compose_helper_f<Function> {
	Function mF;

public:
	compose_helper_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename... Args>
	constexpr auto operator()(Args&&... args) const {
		return mF(std::forward<Args>(args)...);
	}

	template <typename... Args>
	auto operator()(Args&&... args) {
		return mF(std::forward<Args>(args)...);
	}
};

}

/** Composes a sequence of transformations. */
template <typename... Transforms>
struct compose {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		typedef detail::compose_helper_f<typename std::remove_reference<Function>::type, Transforms...> Helper;
		return Helper{f}(std::forward<Args>(args)...);
	}
};

/** Forwards the arguments to f without change. */
struct id {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return std::forward<Function>(f)(std::forward<Args>(args)...);
	}
};

/** Forwards the arguments to the function only if Condition is true. */
template <bool Condition>
struct forward_if;

template <>
struct forward_if<true> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

template <>
struct forward_if<false> {
	template <typename Function, typename... Args>
	constexpr static void transform(Function&&, Args&&...) {
	}
};

/** Flips the first two variables. */
struct flip {
	template <typename Function, typename First, typename Second, typename... Args>
	constexpr static auto transform(Function&& f, First&& first, Second&& second, Args&&... rest) {
		return std::forward<Function>(f)(std::forward<Second>(second),
		                                 std::forward<First>(first),
		                                 std::forward<Args>(rest)...);
	}
};

/** Left cyclic shifts the parameters \a n places. */
template <unsigned N>
struct left_shift {
	template <typename Function, typename First, typename... Args>
	constexpr static auto transform(Function&& f, First&& first, Args&&... rest) {
		static_assert(N < 1 + sizeof...(rest),
		  "Cannot left shift more than the size of the parameter pack");
		return left_shift<N - 1>::transform(std::forward<Function>(f),
		                                    std::forward<Args>(rest)...,
		                                    std::forward<First>(first));
	}
};

template <>
struct left_shift<0u> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... rest) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(rest)...);
	}
};

/** Right cyclic shifts the parameters \a n places. */
template <unsigned N>
struct right_shift {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		static_assert(N < sizeof...(args),
		  "Cannot right shift more than the size of the parameter pack");
		return left_shift<sizeof...(Args) - N>::transform(std::forward<Function>(f),
		                                                  std::forward<Args>(args)...);
	}
};

template <>
struct right_shift<0u> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... rest) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(rest)...);
	}
};

template <int N, bool NotNegative = (N >= 0)>
struct shift;

template <int N>
struct shift<N, true> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return left_shift<N>::transform(std::forward<Function>(f),
		                                std::forward<Args>(args)...);
	}
};

template <int N>
struct shift<N, false> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return right_shift<-N>::transform(std::forward<Function>(f),
		                                  std::forward<Args>(args)...);
	}
};

/** Left cyclic shifts the tail of the parameters \a n places. */
template <unsigned N>
struct left_shift_tail {
	template <typename Function, typename Fixed, typename First, typename... Args>
	constexpr static auto transform(Function&& f, Fixed&& fixed, First&& first, Args&&... rest) {
		static_assert(N < 1 + sizeof...(rest),
		  "Cannot left shift more than the size of the tail of the parameter pack");
		return left_shift_tail<N - 1>::transform(std::forward<Function>(f),
		                                         std::forward<Fixed>(fixed),
		                                         std::forward<Args>(rest)...,
		                                         std::forward<First>(first));
	}
};

template <>
struct left_shift_tail<0u> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... rest) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(rest)...);
	}
};

/** Right cyclic shifts the tail of the parameters \a n places. */
template <unsigned N>
struct right_shift_tail {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		static_assert(N < sizeof...(args) - 1,
		  "Cannot right shift more than the size of the tail of the parameter pack");
		return left_shift_tail<sizeof...(Args) - N - 1>::transform(std::forward<Function>(f),
		                                                           std::forward<Args>(args)...);
	}
};

template <>
struct right_shift_tail<0u> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... rest) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(rest)...);
	}
};

template <int N, bool NotNegative = (N >= 0)>
struct shift_tail;

template <int N>
struct shift_tail<N, true> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return left_shift_tail<N>::transform(std::forward<Function>(f),
		                                     std::forward<Args>(args)...);
	}
};

template <int N>
struct shift_tail<N, false> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return right_shift_tail<-N>::transform(std::forward<Function>(f),
		                                       std::forward<Args>(args)...);
	}
};

/** Drops the first N arguments. */
template <unsigned N>
struct drop {
	template <typename Function, typename First, typename... Args>
	constexpr static auto transform(Function&& f, First&&, Args&&... args) {
		static_assert(N <= sizeof...(args) + 1, "Cannot drop more variables than are passed");
		return drop<N - 1>::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

template <>
struct drop<0u> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

namespace detail {

template <unsigned N, bool ForwardAll>
struct take_helper;

template <unsigned N>
struct take_helper<N, true> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

template <unsigned N>
struct take_helper<N, false> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return compose<left_shift<N>,
		               drop<sizeof...(args) - N>
		              >::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

}

/** Passes only the first N arguments. */
template <unsigned N>
struct take {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		static_assert(N <= sizeof...(args), "Cannot take more parameters that are available");
		return detail::take_helper<N, N == sizeof...(args)>::transform(std::forward<Function>(f),
		                                                               std::forward<Args>(args)...);
	}
};

template <>
struct take<0u> {
	template <typename Function, typename... Args>
	constexpr static void transform(Function&&, Args&&...) {
	}
};

/** Take only the arguments at positions N, N + 1, ..., M - 1, M */
template <int N, int M>
struct slice {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		static_assert(-sizeof...(args) <= N && N <= sizeof...(args),
		  "N is out of bounds");
		static_assert(-sizeof...(args) <= M && M <= sizeof...(args),
		  "M is out of bounds");
		static int const A = (N + sizeof...(args)) % sizeof...(args);
		static int const B = (M + sizeof...(args)) % sizeof...(args);
		static_assert(A <= B, "N must be <= M");
		static_assert(B <= sizeof...(args), "M is out of bounds");
		return m_slice<A, B>::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}

private:
	template <unsigned A, unsigned B>
	struct m_slice {
		template <typename Function, typename... Args>
		constexpr static auto transform(Function&& f, Args&&... args) {
			return compose<left_shift<A>,
			               drop<sizeof...(args) - (B - A)>
			              >::transform(std::forward<Function>(f), std::forward<Args>(args)...);
		}
	};
};

/** Swap the parameters in the positions \a n and \a m. If a number is negative, it is counted from
    the end of the parameter pack. e.g. -1 would be the last parameter. */
template <int N, int M>
struct swap {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		static int const size = sizeof...(args);
		static_assert(-size <= N && N < size,
		  "N is out of bounds");
		static_assert(-size <= M && M < size,
		  "M is out of bounds");
		static int const A = (N + sizeof...(args)) % sizeof...(args);
		static int const B = (M + sizeof...(args)) % sizeof...(args);
		static int const min = A < B ? A : B;
		static int const max = A < B ? B : A;
		return swap_helper<min, max>::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}

private:
	template <int Min, int Max>
	struct swap_helper {
		template <typename Function, typename... Args>
		constexpr static auto transform(Function&& f, Args&&... args) {
			return compose<shift<Min>,
			               shift_tail<Max - Min - 1>,
			               flip,
			               shift_tail<-(Max - Min - 1)>,
			               shift<-Min>
			              >::transform(std::forward<Function>(f), std::forward<Args>(args)...);
		}
	};

	template <int MinMax>
	struct swap_helper<MinMax, MinMax> {
		template <typename Function, typename... Args>
		constexpr static auto transform(Function&& f, Args&&... args) {
			return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
		}
	};
};

template <int... Positions>
struct cycle;

template <>
struct cycle<> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

template <int First>
struct cycle<First> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

template <int First, int Second, int... Rest>
struct cycle<First, Second, Rest...> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return compose<swap<First, Second>, cycle<First, Rest...>>::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

namespace detail {

template <unsigned N>
struct reverse_helper {
	template <typename Function>
	constexpr static auto transform(Function&&) {
	}

	template <typename Function, typename Arg>
	constexpr static auto transform(Function&& f, Arg&& arg) {
		return id::transform(std::forward<Function>(f), std::forward<Arg>(arg));
	}

	template <typename Function, typename First, typename... Args>
	constexpr static auto transform(Function&& f, First&& first, Args&&... args) {
		return compose<swap<N - 1, -N>,
		               reverse_helper<N - 1>
		              >::transform(std::forward<Function>(f),
		                           std::forward<First>(first),
		                           std::forward<Args>(args)...);
	}
};

template <>
struct reverse_helper<0u> {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return id::transform(std::forward<Function>(f), std::forward<Args>(args)...);
	}
};

}

/** Reverse the order of arguments */
struct reverse {
template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		return detail::reverse_helper<sizeof...(args) / 2>::transform(std::forward<Function>(f),
		                                                              std::forward<Args>(args)...);
	}
};

namespace detail {

template <template <class> class Predicate, typename... Passed>
struct filter_helper;

template <template <class> class Predicate, bool Passed, typename... Passed>
struct next_has_passed;

template <template <class> class Predicate, typename... Passed>
struct next_has_passed<Predicate, true, Passed...> {
	template <typename Function, typename Next, typename... ToBeEvaluated>
	constexpr static auto transform(Function&& f, Passed&&... passed, Next&& next, ToBeEvaluated&&... rest) {
		return filter_helper<Predicate, Passed..., Next>::transform(std::forward<Function>(f),
		                                                            std::forward<Passed>(passed)...,
		                                                            std::forward<Next>(next),
		                                                            std::forward<ToBeEvaluated>(rest)...);
	}

	template <typename Function>
	constexpr static auto transform(Function&& f, Passed&&... passed) {
		return std::forward<Function>(f)(std::forward<Passed>(passed)...);
	}
};

template <template <class> class Predicate, typename... Passed>
struct next_has_passed<Predicate, false, Passed...> {
	template <typename Function, typename Next, typename... ToBeEvaluated>
	constexpr static auto transform(Function&& f, Passed&&... passed, Next&&, ToBeEvaluated&&... rest) {
		return filter_helper<Predicate, Passed...>::transform(std::forward<Function>(f),
		                                                      std::forward<Passed>(passed)...,
		                                                      std::forward<ToBeEvaluated>(rest)...);
	}

	template <typename Function>
	constexpr static auto transform(Function&& f, Passed&&... passed) {
		return std::forward<Function>(f)(std::forward<Passed>(passed)...);
	}
};

template <template <class> class Predicate, typename... Passed>
struct filter_helper {
	template <typename Function, typename Next, typename... ToBeEvaluated>
	constexpr static auto transform(Function&& f, Passed&&... passed, Next&& next, ToBeEvaluated&&... rest) {
		typedef next_has_passed<Predicate,
		                        Predicate<Next>::value,
		                        Passed...> NextTransform;
		return NextTransform::transform(std::forward<Function>(f),
		                                std::forward<Passed>(passed)...,
		                                std::forward<Next>(next),
		                                std::forward<ToBeEvaluated>(rest)...);
	}

	template <typename Function, typename... ToBeEvaluated>
	constexpr static auto transform(Function&& f, Passed&&... passed) {
		return std::forward<Function>(f)(std::forward<Passed>(passed)...);
	}
};

}

/** Filter in parameters only if Predicate<Arg>::value is true for each argument type. */
template <template <class> class Predicate>
struct filter {
	template <typename Function, typename... Args>
	constexpr static auto transform(Function&& f, Args&&... args) {
		typedef detail::filter_helper<Predicate> Next;
		return Next::transform(std::forward<Function>(f),
		                       std::forward<Args>(args)...);
	}
};

/**************************************************************************************************
 * Functions                                                                                      *
 **************************************************************************************************/

template <typename Function>
class map_f {
	Function mF;

public:
	constexpr map_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename First, typename... Args>
	constexpr void operator()(First&& first, Args&&... args) const {
		mF(std::forward<First>(first));
		operator()(std::forward<Args>(args)...);
	}

	template <typename First, typename... Args>
	void operator()(First&& first, Args&&... args) {
		mF(std::forward<First>(first));
		operator()(std::forward<Args>(args)...);
	}

	constexpr void operator()() const {
	}
};

template <typename Function>
constexpr map_f<typename std::remove_reference<Function>::type> map(Function&& f) {
	return {std::forward<Function>(f)};
}

template <unsigned N, typename Function>
class adjacent_map_f {
	Function mF;

public:
	constexpr adjacent_map_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename First, typename... Args>
	void operator()(First&& first, Args&&... args) const {
		take<N>::transform(mF, std::forward<First>(first), args...);
		forward_if<(sizeof...(args) >= N)>::transform(*this, std::forward<Args>(args)...);
	}

	template <typename First, typename... Args>
	void operator()(First&& first, Args&&... args) {
		take<N>::transform(mF, std::forward<First>(first), args...);
		forward_if<(sizeof...(args) >= N)>::transform(*this, std::forward<Args>(args)...);
	}
};

template <unsigned N, typename Function>
constexpr adjacent_map_f<N, typename std::remove_reference<Function>::type> adjacent_map(Function&& f) {
	return {std::forward<Function>(f)};
}

template <typename Function>
class foldl_f {
	Function mF;

public:
	constexpr foldl_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename First, typename Second, typename... Args>
	constexpr auto operator()(First&& first, Second&& second, Args&&... args) const {
		return (*this)(mF(std::forward<First>(first), std::forward<Second>(second)),
		                  std::forward<Args>(args)...);
	}

	template <typename First, typename Second, typename... Args>
	auto operator()(First&& first, Second&& second, Args&&... args) {
		return (*this)(mF(std::forward<First>(first), std::forward<Second>(second)),
		                  std::forward<Args>(args)...);
	}

private:
	template <typename Arg>
	constexpr Arg operator()(Arg&& arg) const {
		return std::forward<Arg>(arg);
	}
};

template <typename Function>
constexpr foldl_f<typename std::remove_reference<Function>::type> foldl(Function&& f) {
	return {std::forward<Function>(f)};
}

#if 0
template <typename Function>
class foldr_f {
	foldl_f<Function> mF;

public:
	foldr_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename... Args>
	constexpr auto operator()(Args&&... args) const {
		return forward_after<reverse>(forward_after<flip>(mF))(std::forward<Args>(args)...);
	}

	template <typename... Args>
	auto operator()(Args&&... args) {
		return forward_after<reverse>(forward_after<flip>(mF))(std::forward<Args>(args)...);
	}
};

template <typename Function>
constexpr foldr_f<typename std::remove_reference<Function>::type> foldr(Function&& f) {
	return {std::forward<Function>(f)};
}
#endif

template <typename Function>
class all_of_f {
	Function mF;

public:
	constexpr all_of_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename First, typename... Args>
	constexpr bool operator()(First&& first, Args&&... args) const {
		return mF(std::forward<First>(first)) ? operator()(std::forward<Args>(args)...) : false;
	}

	template <typename First, typename... Args>
	bool operator()(First&& first, Args&&... args) {
		return mF(std::forward<First>(first)) ? operator()(std::forward<Args>(args)...) : false;
	}

	constexpr bool operator()() const {
		return true;
	}
};

template <typename Function>
constexpr all_of_f<typename std::remove_reference<Function>::type> all_of(Function&& f) {
	return {std::forward<Function>(f)};
}

template <typename Function>
class any_of_f {
	Function mF;

public:
	constexpr any_of_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename First, typename... Args>
	constexpr bool operator()(First&& first, Args&&... args) const {
		return mF(std::forward<First>(first)) ? true : operator()(std::forward<Args>(args)...);
	}

	template <typename First, typename... Args>
	bool operator()(First&& first, Args&&... args) {
		return mF(std::forward<First>(first)) ? true : operator()(std::forward<Args>(args)...);
	}

	constexpr bool operator()() const {
		return false;
	}
};

template <typename Function>
constexpr any_of_f<typename std::remove_reference<Function>::type> any_of(Function&& f) {
	return {std::forward<Function>(f)};
}

template <typename Function>
class none_of_f {
	Function mF;

public:
	constexpr none_of_f(Function f)
	: mF(std::move(f)) {
	}

	template <typename First, typename... Args>
	constexpr bool operator()(First&& first, Args&&... args) const {
		return mF(std::forward<First>(first)) ? false : operator()(std::forward<Args>(args)...);
	}

	template <typename First, typename... Args>
	bool operator()(First&& first, Args&&... args) {
		return mF(std::forward<First>(first)) ? false : operator()(std::forward<Args>(args)...);
	}

	constexpr bool operator()() const noexcept {
		return true;
	}
};

template <typename Function>
constexpr none_of_f<typename std::remove_reference<Function>::type> none_of(Function&& f) {
	return {std::forward<Function>(f)};
}

/**************************************************************************************************
 * Utility Functions                                                                              *
 **************************************************************************************************/

template <typename Arg, typename... Args>
constexpr auto head(Arg&& head, Args&&...) noexcept {
	return std::forward<Arg>(head);
}

template <typename Arg>
constexpr auto last(Arg&& arg) noexcept {
	return std::forward<Arg>(arg);
}

template <typename Arg1, typename Arg2, typename... Args>
constexpr auto last(Arg1&&, Arg2&&, Args&&... rest) noexcept {
	return last(std::forward<Args>(rest)...);
}

namespace detail {

template <unsigned N>
struct at_helper;

template <unsigned N>
struct at_helper {
	template <typename Arg, typename... Args>
	constexpr static auto get(Arg&&, Args&&... args) noexcept {
		return at_helper<N - 1>::get(std::forward<Args>(args)...);
	}
};

template <>
struct at_helper<0u> {
	template <typename... Args>
	constexpr static auto get(Args&&... args) noexcept {
		return head(std::forward<Args>(args)...);
	}
};

}

template <int N, typename... Args>
constexpr auto at(Args&&... args) noexcept {
	// GCC won't let me create static const variables in a
	// constexpr function so we are left with this mess below.
	static_assert(-static_cast<int>(sizeof...(args)) <= N && N < static_cast<int>(sizeof...(args)), "N is out of bounds");
	return detail::at_helper<static_cast<unsigned>((N + sizeof...(args)) % sizeof...(args))>::get(std::forward<Args>(args)...);
}

}

#endif
